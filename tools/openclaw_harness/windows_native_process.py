"""Read-only Windows identities and the footing-proven native ConPTY primitive.

No os.kill, signal delivery, process termination, or implicit HPCON teardown of
a live child. ProcessSnapshot has the existing certification inspector shape.
"""
from __future__ import annotations

import base64
import ctypes as c
from ctypes import wintypes as w
from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess
import threading


from certification_process_lease import ProcessSnapshot

class COORD(c.Structure):
    _fields_ = [('X', w.SHORT), ('Y', w.SHORT)]


class SI(c.Structure):
    _fields_ = [('cb', w.DWORD), ('reserved', w.LPWSTR), ('desktop', w.LPWSTR),
        ('title', w.LPWSTR), ('x', w.DWORD), ('y', w.DWORD), ('xsize', w.DWORD),
        ('ysize', w.DWORD), ('xchars', w.DWORD), ('ychars', w.DWORD), ('fill', w.DWORD),
        ('flags', w.DWORD), ('show', w.WORD), ('reserved2size', w.WORD),
        ('reserved2', c.POINTER(w.BYTE)), ('stdin', w.HANDLE),
        ('stdout', w.HANDLE), ('stderr', w.HANDLE)]


class SIEX(c.Structure):
    _fields_ = [('si', SI), ('attrs', c.c_void_p)]


class PI(c.Structure):
    _fields_ = [('process', w.HANDLE), ('thread', w.HANDLE), ('pid', w.DWORD), ('tid', w.DWORD)]


def kernel():
    if os.name != 'nt':
        raise OSError('native Windows backend requires Windows')
    k = c.WinDLL('kernel32', use_last_error=True)
    H = w.HANDLE
    PH = c.POINTER(H)
    signatures = {
        'CreatePipe': ([PH, PH, c.c_void_p, w.DWORD], w.BOOL),
        'CreatePseudoConsole': ([COORD, H, H, w.DWORD, PH], c.c_long),
        'ResizePseudoConsole': ([H, COORD], c.c_long),
        'ClosePseudoConsole': ([H], None),
        'InitializeProcThreadAttributeList': ([c.c_void_p, w.DWORD, w.DWORD, c.POINTER(c.c_size_t)], w.BOOL),
        'UpdateProcThreadAttribute': ([c.c_void_p, w.DWORD, c.c_size_t, c.c_void_p, c.c_size_t, c.c_void_p, c.c_void_p], w.BOOL),
        'DeleteProcThreadAttributeList': ([c.c_void_p], None),
        'CreateProcessW': ([w.LPCWSTR, w.LPWSTR, c.c_void_p, c.c_void_p, w.BOOL, w.DWORD, c.c_void_p, w.LPCWSTR, c.c_void_p, c.POINTER(PI)], w.BOOL),
        'CloseHandle': ([H], w.BOOL),
        'ReadFile': ([H, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p], w.BOOL),
        'WriteFile': ([H, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p], w.BOOL),
        'WaitForSingleObject': ([H, w.DWORD], w.DWORD),
        'GetExitCodeProcess': ([H, c.POINTER(w.DWORD)], w.BOOL),
        'GetProcessTimes': ([H, c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME)], w.BOOL),
        'OpenProcess': ([w.DWORD, w.BOOL, w.DWORD], H),
        'QueryFullProcessImageNameW': ([H, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)], w.BOOL),
    }
    for name, (args, result) in signatures.items():
        func = getattr(k, name)
        func.argtypes, func.restype = args, result
    return k


def checked(value):
    if not value:
        raise c.WinError(c.get_last_error())


def handle_alive(k, handle) -> bool:
    state = k.WaitForSingleObject(handle, 0)
    if state == 0:
        return False
    if state == 258:
        return True
    raise c.WinError(c.get_last_error())


def handle_birth(k, handle) -> int:
    times = [w.FILETIME() for _ in range(4)]
    checked(k.GetProcessTimes(handle, *(c.byref(item) for item in times)))
    return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime


def handle_image(k, handle) -> str:
    size = w.DWORD(32768)
    buf = c.create_unicode_buffer(size.value)
    checked(k.QueryFullProcessImageNameW(handle, 0, buf, c.byref(size)))
    return buf.value


def cim_process(pid: int) -> dict:
    # Only a validated integer is interpolated. Project plain values, avoiding
    # enriched PowerShell strings and unrelated process/environment contents.
    script = ("$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';"
        f"$p=Get-CimInstance Win32_Process -Filter 'ProcessId={int(pid)}';"
        "if($null -eq $p){'null'}else{[ordered]@{pid=[int]$p.ProcessId;"
        "birth_ticks=$p.CreationDate.ToUniversalTime().ToFileTimeUtc();"
        "command=[string]$p.CommandLine;executable=[string]$p.ExecutablePath;"
        "parent_pid=[int]$p.ParentProcessId}|ConvertTo-Json -Compress}")
    encoded = base64.b64encode(script.encode('utf-16le')).decode('ascii')
    result = subprocess.run(['powershell', '-NoProfile', '-NonInteractive',
        '-EncodedCommand', encoded], capture_output=True, text=True,
        errors='replace', timeout=10)
    if result.returncode:
        raise OSError('native CIM identity query failed')
    row = json.loads(result.stdout)
    return row if isinstance(row, dict) else {}


class WindowsProcessInspector:
    """Read-only query/synchronization handle, birth, image and CIM command.

    Access/query failures return alive with incomplete identity, never death.
    The native handle brackets CIM lookup to exclude PID reuse races. Commands
    are cached only for that exact native generation, never for a PID alone.
    """
    def __init__(self):
        self.k = kernel()
        self._commands = {}

    def inspect(self, pid: int) -> ProcessSnapshot:
        if pid <= 0:
            return ProcessSnapshot(pid=pid, alive=False)
        h = self.k.OpenProcess(0x00100000 | 0x1000, False, pid)
        if not h:
            if c.get_last_error() == 87:  # PID absent, not access denied.
                return ProcessSnapshot(pid=pid, alive=False)
            return ProcessSnapshot(pid=pid, alive=True)
        try:
            if not handle_alive(self.k, h):
                return ProcessSnapshot(pid=pid, alive=False)
            birth = handle_birth(self.k, h)
            image = handle_image(self.k, h)
            key = (pid, birth)
            command = self._commands.get(key, '')
            if not command:
                row = cim_process(pid)
                # CIM rounds native creation times to microseconds.
                if row.get('pid') != pid or int(row.get('birth_ticks', 0)) // 10 != birth // 10:
                    return ProcessSnapshot(pid=pid, alive=handle_alive(self.k, h))
                command = str(row.get('command', '')).strip()
                if command:
                    self._commands[key] = command
            if not handle_alive(self.k, h):
                return ProcessSnapshot(pid=pid, alive=False)
            return ProcessSnapshot(pid, True, image, 'windows-filetime:' + str(birth), command)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            return ProcessSnapshot(pid=pid, alive=True)
        finally:
            self.k.CloseHandle(h)

    def signal(self, pid: int, sig: int) -> None:
        raise OSError('Windows identity backend is read-only; native owner must close its child')


class NativeConPTY:
    """Owns one exact native child handle, HPCON and drained VT transcript."""
    def __init__(self, transcript_path: Path, size=(100, 30)):
        self.k = kernel()
        self.transcript_path = Path(transcript_path)
        self.ir, self.iw, self.orr, self.ow, self.pc = [w.HANDLE() for _ in range(5)]
        self.pi = PI()
        self.reader = None
        self.reader_error = ''
        self._closed = False
        try:
            checked(self.k.CreatePipe(c.byref(self.ir), c.byref(self.iw), None, 0))
            checked(self.k.CreatePipe(c.byref(self.orr), c.byref(self.ow), None, 0))
            hr = self.k.CreatePseudoConsole(COORD(*size), self.ir, self.ow, 0, c.byref(self.pc))
            if hr < 0:
                raise OSError('CreatePseudoConsole HRESULT ' + str(hr))
            self.transcript_path.parent.mkdir(parents=True, exist_ok=True)
            # Open before launching; startup output must be drained too.
            self._sink = self.transcript_path.open('xb')
            self.reader = threading.Thread(target=self._drain, name='caol-conpty-reader', daemon=True)
            self.reader.start()
        except BaseException:
            self.close_after_exit()
            raise

    def _drain(self):
        buf = c.create_string_buffer(65536)
        count = w.DWORD()
        try:
            while self.k.ReadFile(self.orr, buf, len(buf), c.byref(count), None) and count.value:
                self._sink.write(buf.raw[:count.value])
                self._sink.flush()
        except OSError as error:
            self.reader_error = str(error)
        finally:
            self._sink.close()

    def launch(self, argv: list[str], cwd: Path):
        if self.pi.process:
            raise RuntimeError('ConPTY already owns a child')
        attrs = None
        try:
            size = c.c_size_t()
            self.k.InitializeProcThreadAttributeList(None, 1, 0, c.byref(size))
            attrs = c.create_string_buffer(size.value)
            checked(self.k.InitializeProcThreadAttributeList(attrs, 1, 0, c.byref(size)))
            checked(self.k.UpdateProcThreadAttribute(attrs, 0, 0x00020016, self.pc, c.sizeof(self.pc), None, None))
            si = SIEX()
            si.si.cb = c.sizeof(SIEX)
            si.attrs = c.cast(attrs, c.c_void_p)
            command = subprocess.list2cmdline(argv)
            checked(self.k.CreateProcessW(argv[0], c.create_unicode_buffer(command), None, None,
                False, 0x00080000, None, str(cwd), c.byref(si), c.byref(self.pi)))
            self.k.CloseHandle(self.pi.thread)
            self.pi.thread = None
            for handle in (self.ir, self.ow):
                self.k.CloseHandle(handle)
                handle.value = None
            return {'schema': 'caol-owned-process-generation-v1', 'pid': int(self.pi.pid),
                'alive': self.alive(), 'executable_path': handle_image(self.k, self.pi.process),
                'birth_identity': 'windows-filetime:' + str(handle_birth(self.k, self.pi.process)),
                'command': command}
        finally:
            if attrs:
                self.k.DeleteProcThreadAttributeList(attrs)

    def alive(self):
        return bool(self.pi.process and handle_alive(self.k, self.pi.process))

    def write(self, data: bytes):
        if not self.alive():
            raise OSError('native child exited before input')
        count = w.DWORD()
        checked(self.k.WriteFile(self.iw, data, len(data), c.byref(count), None))
        if count.value != len(data):
            raise OSError('partial ConPTY input write')

    def resize(self, size):
        hr = self.k.ResizePseudoConsole(self.pc, COORD(*size))
        if hr < 0:
            raise OSError('ResizePseudoConsole HRESULT ' + str(hr))

    def exit_code(self):
        if self.alive():
            return None
        code = w.DWORD()
        checked(self.k.GetExitCodeProcess(self.pi.process, c.byref(code)))
        return code.value

    def close_after_exit(self):
        if self._closed:
            return
        if self.alive():
            raise RuntimeError('retained_live_native_child')
        # ClosePseudoConsole can terminate attached clients; never use it as quit.
        if self.pc:
            self.k.ClosePseudoConsole(self.pc)
            self.pc.value = None
        if self.reader:
            self.reader.join(5)
            if self.reader.is_alive():
                raise RuntimeError('terminal reader exit unobserved; owned handles retained')
        if self.pi.process:
            self.k.CloseHandle(self.pi.process)
            self.pi.process = None  # ctypes structure HANDLE fields yield ints.
        for handle in (self.ir, self.iw, self.orr, self.ow):
            if handle:
                self.k.CloseHandle(handle)
                handle.value = None
        self._closed = True


class WindowsOwnedProcess:
    """Read-only Popen-shaped observation of the broker's exact native child."""
    def __init__(self, generation, args):
        self.pid = int(generation['pid'])
        self.args = args
        self.returncode = None
        self.k = kernel()
        self._guard = threading.Lock()
        self._handle = self.k.OpenProcess(0x00100000 | 0x1000, False, self.pid)
        checked(self._handle)
        if ('windows-filetime:' + str(handle_birth(self.k, self._handle)) != generation['birth_identity']
                or os.path.normcase(handle_image(self.k, self._handle)) != os.path.normcase(generation['executable_path'])):
            self.k.CloseHandle(self._handle)
            self._handle = None
            raise OSError('native process generation changed before observation handle bind')

    def poll(self):
        with self._guard:
            if self.returncode is not None:
                return self.returncode
            if handle_alive(self.k, self._handle):
                return None
            code = w.DWORD()
            checked(self.k.GetExitCodeProcess(self._handle, c.byref(code)))
            self.returncode = int(code.value)
            self.k.CloseHandle(self._handle)
            self._handle = None
            return self.returncode

    def wait(self, timeout=None):
        import time
        deadline = None if timeout is None else time.monotonic() + timeout
        while self.poll() is None:
            if deadline is not None and time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired(self.args, timeout)
            time.sleep(.03)
        return self.returncode

    def terminate(self):
        raise OSError('native terminal child requires its supported quit route; observation never terminates')

    kill = terminate

    def __del__(self):
        handle = getattr(self, '_handle', None)
        if handle:
            self.k.CloseHandle(handle)  # Query handle only, never HPCON or termination.


def lease_kernel():
    k = kernel()
    signatures = {
        'CreateFileW': ([w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE], w.HANDLE),
        'GetFinalPathNameByHandleW': ([w.HANDLE, w.LPWSTR, w.DWORD, w.DWORD], w.DWORD),
        'GetCurrentProcess': ([], w.HANDLE),
        'DuplicateHandle': ([w.HANDLE, w.HANDLE, w.HANDLE, c.POINTER(w.HANDLE), w.DWORD, w.BOOL, w.DWORD], w.BOOL),
        'CreateEventW': ([c.c_void_p, w.BOOL, w.BOOL, w.LPCWSTR], w.HANDLE),
        'OpenEventW': ([w.DWORD, w.BOOL, w.LPCWSTR], w.HANDLE),
        'SetEvent': ([w.HANDLE], w.BOOL),
    }
    for name, (args, result) in signatures.items():
        f = getattr(k, name)
        f.argtypes, f.restype = args, result
    return k


def final_handle_path(k, handle):
    buf = c.create_unicode_buffer(32768)
    size = k.GetFinalPathNameByHandleW(handle, buf, len(buf), 0)
    checked(size)
    if size >= len(buf):
        raise OSError('native lease path exceeds handle path buffer')
    path = buf.value
    if path.startswith('\\\\?\\UNC\\'):
        path = '\\\\' + path[8:]
    elif path.startswith('\\\\?\\'):
        path = path[4:]
    return os.path.normcase(os.path.normpath(path))


class WindowsDirectoryLease:
    """The existing exclusive-root/shared-ancestor contract using share modes.

    FILE_ADD_FILE access participates in write sharing. Ancestor handles share
    READ|WRITE; the root shares READ only. No parent lock files or registry are
    created. Denying DELETE also keeps each opened directory path stable.
    Duplicated handles retain the same file object's share exclusion across
    launcher return; byte-range locks are deliberately not used.
    """
    def __init__(self, root):
        self.k = lease_kernel()
        self.handles = []
        self.paths = []
        self.root = Path(root).absolute()
        try:
            for path in [*reversed(self.root.parents), self.root]:
                if not path.exists():
                    path.mkdir(exist_ok=True)
                if path.resolve() != path or path.is_junction() or path.is_symlink():
                    raise OSError('native directory lease rejects aliases: ' + str(path))
                h = self.k.CreateFileW(str(path), 2, 1 if path == self.root else 3,
                    None, 3, 0x02000000 | 0x00200000, None)
                if h == c.c_void_p(-1).value:
                    raise c.WinError(c.get_last_error())
                self.handles.append(h)
                actual = final_handle_path(self.k, h)
                if actual != os.path.normcase(os.path.normpath(str(path))):
                    raise OSError('native lease resolved path changed: ' + str(path))
                self.paths.append(actual)
        except BaseException:
            self.close()
            raise

    def export(self, *, root_file_handle, root_file_path, generation, run_id):
        if not self.handles:
            raise OSError('native directory lease is closed')
        return {'schema': 'caol-native-writable-lease-v1', 'host': __import__('socket').gethostname(),
            'run_id': run_id, 'userdir': str(self.root), 'owner_generation': generation,
            'handles': [*self.handles, int(root_file_handle)],
            'paths': [*self.paths, os.path.normcase(os.path.normpath(str(root_file_path)))]}

    def close(self):
        for h in reversed(self.handles):
            self.k.CloseHandle(h)
        self.handles.clear()


def adopt_native_lease(export, *, run_id):
    """Duplicate exact launcher-held file objects before any native child exists."""
    from dataclasses import asdict
    import socket
    if not isinstance(export, dict) or export.get('schema') != 'caol-native-writable-lease-v1':
        raise OSError('native writable lease export unavailable')
    if export.get('host') != socket.gethostname() or export.get('run_id') != run_id:
        raise OSError('native writable lease host/run mismatch')
    root = Path(export['userdir']).absolute()
    if root.resolve() != root:
        raise OSError('native writable lease root alias')
    expected = export['owner_generation']
    inspector = WindowsProcessInspector()
    def exact():
        current = asdict(inspector.inspect(int(expected['pid'])))
        return current.get('alive') and all(expected.get(k) and current.get(k) == expected.get(k)
            for k in ('pid', 'birth_identity', 'command', 'executable_path'))
    if not exact():
        raise OSError('native writable lease launcher identity unavailable or changed')
    marker = json.loads((root / '.harness-owner.lock').read_text())
    if marker.get('schema') != 'caol-writable-run-owner-v1' or marker.get('host') != export['host'] \
            or marker.get('run_id') != run_id or marker.get('userdir') != str(root) \
            or marker.get('launch_state') != 'launching' or marker.get('launcher_generation') != expected:
        raise OSError('native writable lease reservation mismatch')
    values = export.get('handles', [])
    paths = export.get('paths', [])
    if not values or len(values) != len(paths) or paths[-1] != os.path.normcase(str(root / '.harness-owner.lock')):
        raise OSError('native writable lease handle export incomplete')
    k = lease_kernel()
    source = k.OpenProcess(0x0040, False, int(expected['pid']))  # PROCESS_DUP_HANDLE only.
    checked(source)
    held = []
    try:
        for value, path in zip(values, paths):
            duplicate = w.HANDLE()
            checked(k.DuplicateHandle(source, int(value), k.GetCurrentProcess(),
                c.byref(duplicate), 0, False, 2))
            held.append(int(duplicate.value))
            if final_handle_path(k, duplicate) != path:
                raise OSError('native writable lease handle path mismatch')
        if not exact():
            raise OSError('native writable lease launcher changed during handle transfer')
        return held
    except BaseException:
        for h in held:
            k.CloseHandle(h)
        raise
    finally:
        k.CloseHandle(source)


def create_semantic_wake_event(name):
    if not name or not name.startswith('Local\\caol-semantic-wake-'):
        raise OSError('native semantic wake event identity invalid')
    k = lease_kernel()
    h = k.CreateEventW(None, False, False, name)
    checked(h)
    if c.get_last_error() == 183:
        k.CloseHandle(h)
        raise OSError('native semantic wake event already exists; ownership not adopted')
    return h


def signal_semantic_wake_event(name):
    k = lease_kernel()
    h = k.OpenEventW(0x0002, False, name)  # EVENT_MODIFY_STATE, no reset/query authority.
    checked(h)
    try:
        checked(k.SetEvent(h))
        return 1
    finally:
        k.CloseHandle(h)


def pipe_readable(descriptor, timeout):
    """Observe an existing anonymous subprocess pipe without a socket select."""
    import msvcrt
    import time
    k = kernel()
    k.PeekNamedPipe.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.POINTER(w.DWORD), c.POINTER(w.DWORD)]
    k.PeekNamedPipe.restype = w.BOOL
    handle = msvcrt.get_osfhandle(descriptor)
    deadline = None if timeout is None else time.monotonic() + max(0, timeout)
    while True:
        available = w.DWORD()
        if not k.PeekNamedPipe(handle, None, 0, None, c.byref(available), None):
            if c.get_last_error() in (109, 232):  # broken/no-data pipe: read observes EOF.
                return True
            raise c.WinError(c.get_last_error())
        if available.value:
            return True
        if deadline is not None and time.monotonic() >= deadline:
            return False
        time.sleep(.01)


def replace_with_readers(source, destination, *, timeout=2.0):
    """Publish one atomic file despite brief Windows read-handle sharing races."""
    import time
    deadline = time.monotonic() + timeout
    while True:
        try:
            os.replace(source, destination)
            return
        except PermissionError as error:
            if getattr(error, 'winerror', None) not in (5, 32) or time.monotonic() >= deadline:
                raise
            time.sleep(.01)
