"""A launch/profile file lease, using existing native run process records.

This is write exclusion for a configured userdir, not a host inventory or source
sync admission. The game inherits the descriptor; launcher loss cannot mean idle.
"""
from __future__ import annotations

import contextlib
import errno
import json
import os
from pathlib import Path
import socket
import stat
from typing import Callable, Mapping


class WritableRootConflict(RuntimeError):
    """A writable-root reservation is active or cannot be trusted."""

    def __init__(self, message: str, *, reason: str = "ownership_conflict"):
        super().__init__(message)
        self.reason = reason


ACTIVE: dict[str, 'WritableRunOwner'] = {}


def _generation_matches(expected: Mapping, observed: Mapping) -> bool:
    try:
        return bool(
            int(expected.get('pid', 0) or 0) > 0
            and int(expected.get('pid', 0) or 0) == int(observed.get('pid', 0) or 0)
            and str(expected.get('birth_identity', '')).strip()
            and str(expected.get('birth_identity', '')).strip()
            == str(observed.get('birth_identity', '')).strip()
            and str(expected.get('command', '')).strip()
            and str(expected.get('command', '')).strip()
            == str(observed.get('command', '')).strip()
        )
    except (TypeError, ValueError):
        return False


def reject_aliases(root: Path) -> Path:
    root = root.absolute()
    if root.resolve() != root:
        raise WritableRootConflict(f"writable root alias rejected: {root}", reason="root_alias")
    # Existing file-only owners predate directory-lock inheritance. Keep their
    # root boundaries too; an incomplete marker is ambiguity, not permission.
    for parent in root.parents:
        if (parent / '.harness-owner.lock').exists():
            raise WritableRootConflict(f"writable root is nested beneath an owner boundary: {parent}",
                                       reason="root_overlap")
    if root.exists():
        # Saves/config/cache subtrees must not escape into another run. This
        # includes installed snapshots, so check again before creating a child.
        for path in root.rglob('*'):
            if path.is_symlink() or (os.name == 'nt' and path.is_junction()):
                raise WritableRootConflict(f"writable subtree alias rejected: {path}", reason="root_alias")
            if path.name == '.harness-owner.lock' and path.parent != root:
                raise WritableRootConflict(f"writable root contains another owner boundary: {path.parent}",
                                           reason="root_overlap")
    return root


class WritableRunOwner:
    def __init__(self, root: Path, run_id: str, inspect: Callable, matches: Callable | None = None):
        import fcntl
        if not str(run_id).strip():
            raise ValueError("writable-root owner requires a nonempty run ID")
        if not callable(inspect):
            raise TypeError("writable-root owner requires a process inspector")
        self.root = reject_aliases(root)
        self.run_id = run_id
        self.inspect = inspect
        # Kept for the existing startup wrapper's API shape. Process identity
        # validation below uses the same PID/birth/command fields directly.
        self.matches = matches
        self._directory_fds = []
        self.handle = None
        try:
            self._acquire_directory_locks()
            if self.root.resolve() != self.root:
                raise WritableRootConflict(f"writable root alias appeared during reservation: {self.root}",
                                           reason="root_alias")
            reject_aliases(self.root)
            self.path = self.root / '.harness-owner.lock'
            # Never unlink this inode: inherited holders and new launchers must
            # contend on the same file after a starter exits.
            existed = self.path.exists() or self.path.is_symlink()
            flags = os.O_RDWR | os.O_CREAT
            if hasattr(os, "O_NOFOLLOW"):
                flags |= os.O_NOFOLLOW
            try:
                fd = os.open(self.path, flags, 0o600)
            except OSError as error:
                if error.errno in (errno.ELOOP, errno.EMLINK):
                    raise WritableRootConflict(f"writable owner lock alias rejected: {self.path}",
                                               reason="lock_alias") from error
                raise
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode):
                os.close(fd)
                raise WritableRootConflict(f"writable owner lock is not a regular file: {self.path}",
                                           reason="malformed_owner")
            os.fchmod(fd, 0o600)
            self.handle = os.fdopen(fd, 'r+b', buffering=0)
            try:
                fcntl.flock(self.handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise WritableRootConflict(
                    f"writable root already has an inherited owner: {self.root}", reason="live_lock",
                ) from error
            self._check_prior_owner(existed=existed)
            launcher_generation = inspect(os.getpid())
            if not self._valid_generation(launcher_generation):
                raise WritableRootConflict("current launcher process generation is incomplete",
                                           reason="identity_unavailable")
            self.record = {'schema': 'caol-writable-run-owner-v1', 'host': socket.gethostname(),
                           'run_id': run_id, 'userdir': str(self.root),
                           'launcher_generation': dict(launcher_generation), 'game_generation': None,
                           'launch_state': 'not_started',
                           'directory_lock_scope': 'exclusive_root_shared_ancestors'}
            self._write()
        except BaseException:
            if self.handle is not None:
                self.handle.close()
            self._close_directory_locks()
            raise

    def _acquire_directory_locks(self):
        import fcntl
        # Read-only directory descriptors avoid parent lock files or a second
        # owner index. A child holds SH on each ancestor; a root holds EX on
        # itself. These exact inode locks reject parent/child claims atomically
        # and remain held by inherited game/broker descriptors after return.
        flags = os.O_RDONLY | os.O_DIRECTORY | getattr(os, 'O_NOFOLLOW', 0)
        for path in [*reversed(self.root.parents), self.root]:
            if not path.exists():
                # The parent's SH lock is already held before this mkdir.
                path.mkdir(exist_ok=True)
            if path.resolve() != path:
                raise WritableRootConflict(f"directory alias appeared during reservation: {path}", reason="root_alias")
            fd = os.open(path, flags)
            try:
                mode = fcntl.LOCK_EX if path == self.root else fcntl.LOCK_SH
                fcntl.flock(fd, mode | fcntl.LOCK_NB)
            except BlockingIOError as error:
                os.close(fd)
                raise WritableRootConflict(f"writable directory overlaps an inherited owner: {path}",
                                           reason="live_lock" if path == self.root else "root_overlap") from error
            except BaseException:
                os.close(fd)
                raise
            self._directory_fds.append(fd)

    def _close_directory_locks(self):
        for fd in reversed(self._directory_fds):
            os.close(fd)
        self._directory_fds.clear()

    @staticmethod
    def _valid_generation(generation) -> bool:
        if not isinstance(generation, Mapping):
            return False
        if generation.get('schema') != 'caol-owned-process-generation-v1':
            return False
        try:
            pid = int(generation.get('pid', 0) or 0)
        except (TypeError, ValueError):
            return False
        return bool(pid > 0 and str(generation.get('birth_identity', '')).strip()
                    and str(generation.get('command', '')).strip())

    def _inspect_generation(self, generation, *, source: str) -> None:
        if not self._valid_generation(generation):
            raise WritableRootConflict(f"incomplete process generation in {source}: {generation!r}",
                                       reason="incomplete_generation")
        try:
            observed = self.inspect(int(generation['pid']))
        except Exception as error:
            raise WritableRootConflict(f"process identity inspection failed in {source}: {generation!r}",
                                       reason="identity_unavailable") from error
        if not isinstance(observed, Mapping) or not isinstance(observed.get('alive'), bool):
            raise WritableRootConflict(f"process identity unavailable in {source}: {generation!r}",
                                       reason="identity_unavailable")
        if observed['alive']:
            matcher = self.matches if callable(self.matches) else _generation_matches
            try:
                matched = matcher(generation, observed)
            except Exception as error:
                raise WritableRootConflict(f"process identity comparison failed in {source}: {generation!r}",
                                           reason="identity_unavailable") from error
            if matched:
                raise WritableRootConflict(f"live prior owner retained in {source}: {generation}",
                                           reason="live_generation")
            raise WritableRootConflict(f"PID birth/command changed for prior owner in {source}: {generation}",
                                       reason="generation_mismatch")

    def _check_prior_owner(self, *, existed: bool):
        self.handle.seek(0)
        raw = self.handle.read()
        if raw:
            try:
                prior = json.loads(raw)
            except (ValueError, UnicodeError) as error:
                raise WritableRootConflict(f"malformed owner retained: {self.path}",
                                           reason="malformed_owner") from error
            if not isinstance(prior, dict) or prior.get('schema') != 'caol-writable-run-owner-v1':
                raise WritableRootConflict(f"invalid owner schema retained: {self.path}",
                                           reason="malformed_owner")
            if prior.get('host') != socket.gethostname() or prior.get('userdir') != str(self.root):
                raise WritableRootConflict(f"wrong host/root owner retained: {self.path}",
                                           reason="wrong_owner_scope")
            if not str(prior.get('run_id', '')).strip():
                raise WritableRootConflict(f"incomplete owner record retained: {self.path}",
                                           reason="incomplete_owner")
            if prior.get('launcher_state') != 'returned':
                raise WritableRootConflict(f"previous launcher has not returned: {self.path}",
                                           reason="incomplete_owner")
            launch_state = prior.get('launch_state')
            if launch_state not in ('not_started', 'launching', 'bound'):
                raise WritableRootConflict(f"missing or invalid launch state retained: {self.path}",
                                           reason="incomplete_owner")
            launcher_generation = prior.get('launcher_generation')
            if not self._valid_generation(launcher_generation):
                raise WritableRootConflict(f"incomplete launcher generation retained: {self.path}",
                                           reason="incomplete_generation")
            game_generation = prior.get('game_generation')
            if launch_state == 'not_started':
                if game_generation is not None:
                    raise WritableRootConflict(f"not-started owner unexpectedly has a game binding: {self.path}",
                                               reason="malformed_owner")
            elif launch_state == 'launching':
                raise WritableRootConflict(f"unbound prior game owner retained: {self.path}",
                                           reason="incomplete_owner")
            else:
                if game_generation is None:
                    raise WritableRootConflict(f"bound owner lacks game generation: {self.path}",
                                               reason="incomplete_generation")
                self._inspect_generation(game_generation, source=str(self.path))
        elif existed:
            # Empty files are only valid while this instance has just created
            # the stable lock inode. An existing empty record is interrupted
            # or malformed ownership evidence and must not be overwritten.
            raise WritableRootConflict(f"empty owner record retained: {self.path}",
                                       reason="incomplete_owner")
        # Existing launches before this lease was added still have authority.
        legacy_runs = self.root / 'harness_runs'
        if legacy_runs.is_symlink():
            raise WritableRootConflict(f"legacy run evidence alias rejected: {legacy_runs}",
                                       reason="root_alias")
        for path in legacy_runs.glob('*/process.json'):
            try:
                value = json.loads(path.read_text())
                chain = path.parent / 'process_chain.json'
                generation = (json.loads(chain.read_text())['current_process_generation'] if chain.exists()
                              else value.get('process_generation'))
                if not generation:
                    pid = int(value.get('pid', 0))
                    if pid > 0:
                        observed = self.inspect(pid)
                        if not isinstance(observed, Mapping) or not isinstance(observed.get('alive'), bool):
                            raise WritableRootConflict(f"process identity unavailable for run: {path}",
                                                       reason="identity_unavailable")
                        if observed['alive']:
                            raise WritableRootConflict(f"birth identity unavailable for live run: {path}",
                                                       reason="incomplete_generation")
                else:
                    self._inspect_generation(generation, source=str(path))
            except WritableRootConflict:
                raise
            except (ValueError, KeyError, TypeError, OSError) as error:
                raise WritableRootConflict(f"existing run identity unavailable: {path}",
                                           reason="identity_unavailable") from error

    def _write(self):
        self.handle.seek(0)
        self.handle.truncate()
        self.handle.write(json.dumps(self.record, sort_keys=True).encode())
        self.handle.flush()
        os.fsync(self.handle.fileno())

    @property
    def fd(self):
        return self.handle.fileno()

    def bind_game(self, generation):
        if self.record.get('launch_state') != 'launching':
            raise WritableRootConflict(f"game bind requires begin_launch at {self.root}",
                                       reason="invalid_launch_transition")
        if not self._valid_generation(generation):
            raise WritableRootConflict(f"game generation unavailable; retain run at {self.root}: {generation}",
                                       reason="identity_unavailable")
        try:
            observed = self.inspect(int(generation['pid']))
        except Exception as error:
            raise WritableRootConflict(f"game process generation unavailable at {self.root}: {generation}",
                                       reason="identity_unavailable") from error
        matcher = self.matches if callable(self.matches) else _generation_matches
        try:
            exact = isinstance(observed, Mapping) and observed.get('alive') is True \
                and matcher(generation, observed)
        except Exception as error:
            raise WritableRootConflict(f"game process generation comparison unavailable at {self.root}",
                                       reason="identity_unavailable") from error
        if not exact:
            raise WritableRootConflict(f"game process generation changed before binding at {self.root}: {generation}",
                                       reason="generation_mismatch")
        self.record['game_generation'] = dict(generation)
        self.record['launch_state'] = 'bound'
        self._write()

    def begin_launch(self):
        """Persist possible process creation before invoking native Popen."""
        if self.record.get('launch_state') != 'not_started':
            raise WritableRootConflict(f"launch already started or bound at {self.root}",
                                       reason="invalid_launch_transition")
        self.record['launch_state'] = 'launching'
        self._write()

    @property
    def inherited_fds(self):
        return (self.fd, *self._directory_fds)

    def close(self):
        try:
            self.record['launcher_state'] = 'returned'
            self._write()
        finally:
            self.handle.close()  # Inherited game/broker descriptors retain exclusion.
            self._close_directory_locks()


@contextlib.contextmanager
def own_writable_root(root: Path, run_id: str, inspect: Callable, matches: Callable | None = None):
    if os.name not in ('posix', 'nt'):
        raise OSError('native writable-root ownership unavailable on this platform')
    key = str(reject_aliases(root))
    if key in ACTIVE:
        owner = ACTIVE[key]
        if owner.run_id != run_id:
            raise WritableRootConflict(f"different in-process run owns {root}", reason="in_process_collision")
        yield owner
        return
    owner_class = WindowsWritableRunOwner if os.name == 'nt' else WritableRunOwner
    owner = owner_class(root, run_id, inspect, matches)
    ACTIVE[key] = owner
    try:
        yield owner
    finally:
        ACTIVE.pop(key, None)
        owner.close()


def current_owner(root: Path):
    return ACTIVE.get(str(root.absolute()))


class WindowsWritableRunOwner(WritableRunOwner):
    """Same persisted owner/state transitions, native transferable share leases."""
    def __init__(self, root, run_id, inspect, matches=None):
        from windows_native_process import WindowsDirectoryLease
        if not str(run_id).strip() or not callable(inspect):
            raise ValueError('native writable owner requires run ID and inspector')
        self.root = reject_aliases(root)
        self.run_id, self.inspect, self.matches = run_id, inspect, matches
        self.handle = None
        self._native_lease = None
        try:
            try:
                self._native_lease = WindowsDirectoryLease(self.root)
            except OSError as error:
                raise WritableRootConflict('native writable directory overlaps or is unavailable: ' + str(self.root),
                    reason='root_overlap' if getattr(error, 'winerror', None) == 32 else 'native_lease_unavailable') from error
            reject_aliases(self.root)
            self.path = self.root / '.harness-owner.lock'
            existed = self.path.exists()
            self.handle = os.fdopen(os.open(self.path, os.O_CREAT | os.O_RDWR | os.O_BINARY, 0o600), 'r+b', buffering=0)
            if not stat.S_ISREG(os.fstat(self.fd).st_mode):
                raise WritableRootConflict('native writable owner is not a regular file', reason='malformed_owner')
            self._check_prior_owner(existed=existed)
            launcher = inspect(os.getpid())
            if not self._valid_generation(launcher) or not launcher.get('executable_path'):
                raise WritableRootConflict('native launcher process generation incomplete', reason='identity_unavailable')
            self.record = {'schema': 'caol-writable-run-owner-v1', 'host': socket.gethostname(),
                'run_id': run_id, 'userdir': str(self.root), 'launcher_generation': dict(launcher),
                'game_generation': None, 'launch_state': 'not_started',
                'directory_lock_scope': 'exclusive_root_shared_ancestors',
                'native_lease_kind': 'transferable_directory_share_handles'}
            self._write()
        except BaseException:
            if self.handle is not None:
                self.handle.close()
            if self._native_lease:
                self._native_lease.close()
            raise

    @property
    def inherited_fds(self):
        return ()  # Never represent native HANDLEs as POSIX descriptors.

    def bind_game(self, generation, *, native_owner_path=None):
        try:
            return super().bind_game(generation)
        except WritableRootConflict as error:
            if error.reason != 'generation_mismatch' or native_owner_path is None:
                raise
            observed = self.inspect(int(generation['pid']))
            if not isinstance(observed, Mapping) or observed.get('alive') is not False:
                raise
            path = Path(native_owner_path).resolve()
            if not path.is_relative_to(self.root):
                raise
            # A fast child may exit before this caller acquires a query handle.
            # The existing broker owns its creation handle and publishes the
            # exact exit/closed-handles record, never just a sampled dead PID.
            import time
            deadline = time.monotonic() + 6.0
            while True:
                owner = json.loads(path.read_text())
                if owner.get('launch_state') == 'exited':
                    break
                if time.monotonic() >= deadline:
                    raise error
                time.sleep(.05)
            if owner.get('schema') != 'caol-curses-terminal-owner-v1' \
                    or owner.get('host') != self.record['host'] \
                    or owner.get('run_id') != self.run_id \
                    or owner.get('game_process_generation') != dict(generation) \
                    or owner.get('native_handles_closed') is not True \
                    or owner.get('native_lease_handles_closed') is not True \
                    or 'child_exit_code' not in owner \
                    or self.inspect(int(generation['pid'])).get('alive') is not False:
                raise error
            self.record['game_generation'] = dict(generation)
            self.record['launch_state'] = 'bound'
            self._write()

    def export_native_lease(self):
        import msvcrt
        if self.record['launch_state'] != 'launching':
            raise WritableRootConflict('native lease export requires begin_launch', reason='invalid_launch_transition')
        return self._native_lease.export(root_file_handle=msvcrt.get_osfhandle(self.fd),
            root_file_path=self.path, generation=self.record['launcher_generation'], run_id=self.run_id)

    def close(self):
        try:
            self.record['launcher_state'] = 'returned'
            self._write()
        finally:
            self.handle.close()
            self._native_lease.close()  # Exact duplicated broker handles retain exclusion.
