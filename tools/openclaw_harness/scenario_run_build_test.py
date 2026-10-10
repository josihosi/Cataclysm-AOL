"""Public save-only selection and one resolved run-build controls (no native input)."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

import scenario_registry_cli as cli
import startup_harness as harness
import scenario_registry_cli_test as fixtures
from scenario_registry_store import open_registry, reload_bootstrap_token_for_launch, reload_selection_token_for_launch


class ScenarioRunBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Readiness executes --version: use a native binary on every host.
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        cls.native_builds = {}
        compiler = (shutil.which(os.environ['CXX']) if os.environ.get('CXX') else
                    shutil.which('c++') or shutil.which('g++') or shutil.which('clang++'))
        if not compiler:
            raise RuntimeError('The native run-build fixture requires the host C++ compiler')
        for label in ('one', 'two'):
            source = Path(temporary.name) / ('version-' + label + '.cpp')
            executable = source.with_suffix('.exe' if os.name == 'nt' else '')
            source.write_text(
                '#include <cstdio>\n#include <cstring>\n'
                'int main(int argc, char **argv) {\n'
                '  if (argc != 2 || std::strcmp(argv[1], "--version")) return 2;\n'
                '  std::puts(' + json.dumps(harness.current_head_short()) + ');\n'
                '  std::puts(' + json.dumps('# fixture ' + label) + ');\n'
                '  return 0;\n}\n', encoding='utf-8',
            )
            command = [compiler, '-O2']
            if os.name == 'nt':
                command.append('-static')
            command += [str(source), '-o', str(executable)]
            built = subprocess.run(command, capture_output=True, text=True)
            if built.returncode:
                raise RuntimeError(f'Fixture compile failed: {command!r}\n{built.stdout}\n{built.stderr}')
            cls.native_builds[label] = executable

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.registry = self.root / 'registry.sqlite3'
        self.scenario = self.root / 'scenario.json'
        self.save = self.root / 'save'; self.save.mkdir()
        (self.save / 'avatar').write_bytes(b'unchanged saved player')
        declaration = fixtures.ScenarioRegistryCliTest().strict_manifest()
        declaration['name'] = 'same-save'
        declaration['runtime_contract']['requirements'].pop('executable')
        declaration['runtime_contract']['requirements']['fixture'] = str(self.save)
        self.declaration = declaration
        self.scenario.write_text(json.dumps(declaration))
        self.scenario_bytes = self.scenario.read_bytes()
        self.query = fixtures.ScenarioRegistryCliTest().bootstrap_request()
        self.call('rebuild', '--source', str(self.scenario))
        self.receipts = [self.build('one'), self.build('two')]

    def build(self, label):
        executable = self.root / ('game-' + label + self.native_builds[label].suffix)
        shutil.copy2(self.native_builds[label], executable)
        receipt = self.root / ('receipt-' + label + '.json')
        receipt.write_text(json.dumps({
            'schema': harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
            'captured_head': harness.current_head_short(),
            'executable_path': str(executable),
            'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
            'product_source_sha256': hashlib.sha256(label.encode()).hexdigest(),
            'build_configuration': {'renderer': 'curses', 'make_variables': ['TILES=0','SOUND=0']},
        }))
        return receipt

    def call(self, *argv, okay=True):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = cli.main(['--json', '--registry', str(self.registry), *argv])
        self.assertEqual(code == 0, okay, (out.getvalue(), err.getvalue()))
        return json.loads(out.getvalue() if code == 0 else err.getvalue())

    def select(self, receipt, command='registry-query', brief=None):
        argv=[command, '--query-json', json.dumps(self.query), '--run-build-receipt', str(receipt)]
        if command == 'registry-query':
            p=self.root / 'brief.json'
            p.write_text(json.dumps(brief or {'outcome': 'Observe the native journey'}))
            argv += ['--coordinator-brief',str(p)]
        return self.call(*argv)['result']

    def test_native_builds_execute_version_and_have_distinct_identity(self):
        hashes = []
        for label, receipt in zip(('one', 'two'), self.receipts):
            binding = json.loads(receipt.read_text())
            executable = binding['executable_path']
            version = subprocess.run([executable, '--version'], capture_output=True, text=True)
            self.assertEqual(version.returncode, 0, version.stderr)
            self.assertEqual(version.stdout.splitlines(), [harness.current_head_short(), '# fixture ' + label])
            denied = subprocess.run([executable], capture_output=True, text=True)
            self.assertEqual(denied.returncode, 2)
            self.assertEqual(denied.stdout, '')
            hashes.append(binding['executable_sha256'])
        self.assertNotEqual(*hashes)

    def test_same_unchanged_save_two_builds_through_query_reload_and_probe(self):
        tokens=[]
        for receipt in self.receipts:
            selected=self.select(receipt)
            self.assertTrue(selected['token_id'],selected)
            self.assertEqual(selected['source_executable_readiness']['status'],'ready')
            self.assertEqual(selected['next_action']['kind'],'launch_selected_scenario')
            self.assertNotIn('--witness-charter', selected['next_action']['command']['argv'])
            with contextlib.closing(open_registry(str(self.registry))) as db:
                token=reload_selection_token_for_launch(db,selected['token_id'])
            self.assertTrue(token.accepted,token)
            self.assertEqual(token.scenario, self.declaration['name'])
            ns=cli._registry_launch_probe_namespace(token)
            expected=json.loads(receipt.read_text())['executable_path']
            self.assertEqual(ns.executable,expected)
            self.assertEqual(ns.registry_selected_product_build['receipt_path'],str(receipt))
            self.assertEqual(cli._selected_runtime_binding(Path(expected),token)['executable_path'],expected)
            tokens.append(token.token_id)
        self.assertNotEqual(*tokens)
        self.assertEqual(self.scenario.read_bytes(),self.scenario_bytes)
        self.assertEqual((self.save/'avatar').read_bytes(),b'unchanged saved player')

    def test_actual_bootstrap_and_startup_parser_use_run_receipt_not_legacy_scenario_build(self):
        # A retained obsolete build field cannot select a different build.
        self.declaration['runtime_contract']['requirements']['executable']='missing-old-game'
        self.declaration['runtime_contract']['selected_product_build']={'obsolete':True}
        self.scenario.write_text(json.dumps(self.declaration))
        self.call('rebuild','--source',str(self.scenario))
        selected=self.select(self.receipts[1],command='registry-bootstrap')
        self.assertTrue(selected['accepted'],selected)
        with contextlib.closing(open_registry(str(self.registry))) as db:
            token=reload_bootstrap_token_for_launch(db,selected['token_id'])
        self.assertEqual(selected['scenario'], self.declaration['name'])
        self.assertEqual(token.scenario, self.declaration['name'])
        ns=cli._registry_bootstrap_probe_namespace(token)
        ns.dry_run=True
        ns.registry_launch_receipt=json.dumps({'runtime_binding':token.runtime_binding})
        parsed=[]
        def harmless_start(argv,**kwargs):
            start=harness.build_parser().parse_args(argv[2:])
            parsed.append(start)
            self.assertEqual(start.executable,ns.executable)
            self.assertEqual(json.loads(start.registry_launch_receipt)['runtime_binding'],token.runtime_binding)
            self.assertEqual(harness.compare_runtime_binding(token.runtime_binding)['status'],'matched')
            with mock.patch.object(harness,'_run_startup',return_value=0) as body:
                self.assertEqual(harness.run_startup(start),0)
                body.assert_called_once()
            return 0, {'ok':True}, '', ''
        with mock.patch.object(harness,'run_startup_in_process',side_effect=harmless_start), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(harness.run_probe_mode(ns),0)
        self.assertEqual(len(parsed),1)

    def test_outcome_prose_is_not_second_selector_or_launch_hash_gate(self):
        selected=self.select(self.receipts[0],brief={'outcome':'Try the journey','scenario':'unrelated prose name','query':{'not':'typed request'}})
        with contextlib.closing(open_registry(str(self.registry))) as db:
            token=reload_selection_token_for_launch(db,selected['token_id'],witness_charter={'claim':'Changed wording'})
        self.assertTrue(token.accepted,token)
        self.assertEqual(token.outcome_brief['outcome'],'Try the journey')
        environment=cli._witness_launch_environment(type('Args',(),{'outcome_brief':token.outcome_brief,'witness_charter':None})())
        self.assertEqual(json.loads(environment['OPENCLAW_PLAYTEST_WITNESS_CHARTER'])['claim'],'Try the journey')

    def test_accepted_outcome_alias_reaches_selected_token_and_sealed_journal(self):
        from cockpit import CockpitRunChannel
        from cockpit_witness_test import frame

        for alias in ('outcome', 'desired_outcome', 'claim'):
            with self.subTest(alias=alias):
                brief = {alias: ' Observe the selected journey ',
                         'material_proof': 'Retained bound observations',
                         'current_uncertainty': 'The journey remains unproved'}
                selected = self.select(self.receipts[0], brief=brief)
                with contextlib.closing(open_registry(str(self.registry))) as db:
                    token = reload_selection_token_for_launch(db, selected['token_id'])
                self.assertTrue(token.accepted, token)
                environment = cli._witness_launch_environment(type('Args', (), {
                    'outcome_brief': token.outcome_brief, 'witness_charter': None,
                })())
                charter = json.loads(environment['OPENCLAW_PLAYTEST_WITNESS_CHARTER'])
                channel = CockpitRunChannel(
                    frame, binding_id='selected-alias-binding', witness_charter=charter,
                    witness_identity={
                        'scenario_id': token.scenario,
                        'source_identity': token.source_sha256,
                        'executable_identity': token.runtime_binding['executable_sha256'],
                    },
                )
                observed = channel.observe()
                channel._read_process_state = lambda: {'run_id': 'run-a', 'pid': 123, 'alive': False}
                sealed = channel.seal_witness_journal(
                    observation_id=observed['observation_id'],
                    stop_reason='diagnostic outcome still unproved', unused_authority='none',
                )
                self.assertTrue(sealed['ok'], sealed)
                self.assertEqual(sealed['result']['charter']['claim'], 'Observe the selected journey')
                journal = sealed['result']['evidence_journal']
                self.assertEqual(journal['charter_id'], charter['charter_id'])
                self.assertEqual(journal['identity']['source_identity'], token.source_sha256)
                self.assertEqual(journal['identity']['executable_identity'],
                                 token.runtime_binding['executable_sha256'])
                self.assertEqual(token.outcome_brief['outcome'], 'Observe the selected journey')
                if alias != 'outcome':
                    self.assertEqual(token.outcome_brief[alias], brief[alias])
                self.assertEqual(sealed['result']['charter']['current_uncertainty'],
                                 brief['current_uncertainty'])

    def test_legacy_alias_context_uses_same_precedence_without_rewriting_receipt(self):
        for context in (
            {'desired_outcome': 'Observe the selected journey'},
            {'claim': 'Observe the selected journey'},
            {'desired_outcome': 'Observe the selected journey', 'claim': 'Other legacy prose'},
            {'outcome': 'Observe the selected journey', 'desired_outcome': 'Other legacy prose'},
        ):
            with self.subTest(context=context):
                retained = dict(context)
                environment = cli._witness_launch_environment(type('Args', (), {
                    'outcome_brief': context, 'witness_charter': None,
                })())
                charter = json.loads(environment['OPENCLAW_PLAYTEST_WITNESS_CHARTER'])
                self.assertEqual(charter['claim'], 'Observe the selected journey')
                self.assertEqual(context, retained)

    def test_changed_run_binary_refuses_before_probe_and_same_token_cannot_replay(self):
        selected=self.select(self.receipts[0])
        with contextlib.closing(open_registry(str(self.registry))) as db:
            token=reload_selection_token_for_launch(db,selected['token_id'])
        game=Path(token.runtime_binding['executable_path'])
        game.write_bytes(game.read_bytes()+b'# changed\n')
        with mock.patch.object(harness,'run_probe_mode',side_effect=AssertionError('must not launch')):
            result=self.call('registry-launch',token.token_id,okay=False)
        self.assertEqual(result['result']['reason'],'source_matching_executable_required')
        with contextlib.closing(open_registry(str(self.registry))) as db:
            again=reload_selection_token_for_launch(db,token.token_id)
        self.assertFalse(again.accepted)

    def test_changed_scenario_save_declaration_refuses_without_new_runtime_selection(self):
        selected=self.select(self.receipts[0])
        self.declaration['runtime_contract']['requirements']['fixture']='another-save'
        self.scenario.write_text(json.dumps(self.declaration))
        with contextlib.closing(open_registry(str(self.registry))) as db:
            token=reload_selection_token_for_launch(db,selected['token_id'])
        self.assertFalse(token.accepted)
        self.assertEqual(token.reason,'manifest_source_changed')

    def test_actual_saved_bytes_drift_refuses_preflight_without_claim_or_dispatch(self):
        declaration = dict(self.declaration)
        declaration['steps'] = [{'label':'setup','kind':'native_semantic_bootstrap'}, {'label':'production','kind':'cockpit_live_session'}]
        declaration['proof_route'] = {k:['production'] for k in declaration['proof_route']}
        declaration['saved_world_snapshot'] = str(self.save)
        declaration['world'] = 'world'
        declaration['profile'] = 'disposable-test'
        self.scenario.write_text(json.dumps(declaration))
        self.call('rebuild','--source',str(self.scenario))
        selected = self.select(self.receipts[0])
        with contextlib.closing(open_registry(str(self.registry))) as db:
            token = reload_selection_token_for_launch(db, selected['token_id'])
        destination = self.root/'writable'/'world'
        destination.mkdir(parents=True)
        (destination/'avatar').write_bytes(b'foreign mutable save bytes')
        args=type('Args',(),{'profile':'disposable-test','saved_world_snapshot':'',
                           'post_relaunch_continuation':False})()
        with mock.patch.object(harness,'save_dir_for_profile',return_value=destination.parent):
            with self.assertRaisesRegex(cli.ScenarioRegistryStoreError,'different data'):
                cli._preflight_selected_save_setup(token,args)
        with contextlib.closing(open_registry(str(self.registry))) as db:
            self.assertTrue(reload_selection_token_for_launch(db,token.token_id).accepted)
        self.assertEqual((self.save/'avatar').read_bytes(),b'unchanged saved player')

    def test_complete_public_launch_carries_one_run_binding_to_start_before_mutation(self):
        # The selected declaration need not be named after its scenario identity.
        selected=self.select(self.receipts[0])
        captured=[]
        def harmless_probe(namespace):
            namespace.dry_run=True
            receipt=json.loads(namespace.registry_launch_receipt)
            captured.append(receipt)
            def harmless_start(argv,**kwargs):
                args=harness.build_parser().parse_args(argv[2:])
                self.assertEqual(args.scenario_identity, self.declaration['name'])
                self.assertEqual(args.scenario_contract_path, str(self.scenario))
                self.assertEqual(args.scenario_source_sha256,
                                 hashlib.sha256(self.scenario_bytes).hexdigest())
                with mock.patch.object(harness,'_run_startup',return_value=0) as body:
                    self.assertEqual(harness.run_startup(args),0)
                    body.assert_called_once()
                return 0,{'ok':True},'',''
            with mock.patch.object(harness,'run_startup_in_process',side_effect=harmless_start):
                return real_probe(namespace)
        real_probe=harness.run_probe_mode
        with mock.patch.object(harness,'run_probe_mode',side_effect=harmless_probe):
            self.call('registry-launch',selected['token_id'])
        self.assertEqual(len(captured),1)
        self.assertEqual(captured[0]['runtime_binding']['selected_product_build']['receipt_path'],str(self.receipts[0]))
        self.assertEqual(captured[0]['source_path'],str(self.scenario))
        self.assertEqual(self.scenario.read_bytes(), self.scenario_bytes)
        self.assertFalse((self.root / 'same-save.json').exists())

    def test_run_build_receipt_and_terminal_configuration_are_bound(self):
        binding=cli._resolve_run_build(type('Args',(),{'run_build_receipt':str(self.receipts[0])})())
        scenario={'playtest_mode':'terminal','steps':[{'kind':'native_semantic_bootstrap'}]}
        # Use existing mode declaration schema, never an executable from scenario.
        scenario['runtime_contract']={'playtest_mode':'terminal'}
        with mock.patch.object(harness,'scenario_playtest_mode',return_value='terminal'):
            ready=harness.terminal_mode_readiness(scenario,Path(binding['executable_path']),selected_product_build=binding['selected_product_build'])
        self.assertEqual(ready['status'],'ready',ready)
        self.receipts[0].write_text('{}')
        self.assertEqual(harness.compare_runtime_binding(binding)['status'],'mismatch')

if __name__=='__main__': unittest.main()
