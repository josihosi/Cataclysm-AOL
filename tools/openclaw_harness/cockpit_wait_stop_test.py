"""Portable receipt/owner regression controls for ordinary native wait stopping.

Disposable native-shaped rows; connected actual factory/FIFO/report control is
retained with the chaperone evidence. These controls assert no native gameplay.
"""
from copy import deepcopy
import unittest
from cockpit import CockpitRunChannel

class NativeStopTest(unittest.TestCase):
    def rows(self):
        operation = {'run_id':'run', 'state':'awaiting_decision',
                     'accepted_receipt':{'sequence':10}, 'start_game_minutes':100,
                     'requested_duration_game_minutes':120}
        prompt = {'kind':'prompt','run_id':'run','process_instance':'native',
                  'frame_id':'prompt','surface_id':'owner','sequence':20,
                  'payload':{'title':'CANCEL_ACTIVITY_OR_IGNORE_QUERY'},
                  'valid_actions':[{'id':'prompt.choose','stable_id':'fresh-yes',
                                    'label':'YES','enabled':True}]}
        request = {'action':'game.act','action_id':'prompt.choose',
                   'stable_id':'fresh-yes','observation_id':'prompt'}
        receipt = {'accepted':True,'action_id':'prompt.choose','run_id':'run',
                   'requested_run_id':'run','process_instance':'native','sequence':22,
                   'requested_surface_id':'owner','consuming_surface_id':'owner',
                   'requested_frame_id':'prompt','consuming_frame_id':'prompt',
                   'resulting_frame_id':'world','game_minutes':113,'game_turn':6780}
        world = {'kind':'world','run_id':'run','process_instance':'native',
                 'frame_id':'world','sequence':21,'game_minutes':113,'game_turn':6780}
        return operation,prompt,request,receipt,world

    def test_partial_stop_is_cancelled_not_duration_completion(self):
        operation,prompt,request,receipt,world=self.rows()
        cancelled=CockpitRunChannel._stopped_wait_operation(operation,prompt,request,receipt,world)
        self.assertEqual(cancelled['state'],'cancelled')
        self.assertEqual(cancelled['completed_progress_game_minutes'],13)
        self.assertEqual(cancelled['requested_duration_game_minutes'],120)
        self.assertEqual(cancelled['accepted_receipt'],operation['accepted_receipt'])
        self.assertFalse(cancelled['termination']['duration_fulfilled'])
        self.assertNotIn('termination',operation)

    def test_foreign_receipt_or_nonstop_option_cannot_settle_wait(self):
        for field,value in [('run_id','foreign'),('process_instance','foreign'),
                            ('accepted',False),('consuming_frame_id','old'),
                            ('resulting_frame_id','wrong')]:
            with self.subTest(field=field):
                o,p,q,r,w=self.rows();r[field]=value
                with self.assertRaisesRegex(ValueError,'owner_or_receipt_unproved'):
                    CockpitRunChannel._stopped_wait_operation(o,p,q,r,w)
        for label in ('NO','IGNORE','MANAGER'):
            with self.subTest(label=label):
                o,p,q,r,w=self.rows();p['valid_actions'][0]['label']=label
                with self.assertRaises(ValueError):CockpitRunChannel._stopped_wait_operation(o,p,q,r,w)

    def test_completed_or_non_scalar_wait_does_not_own_later_stop(self):
        for duration in (None,5,13):
            with self.subTest(duration=duration):
                o,p,q,r,w=self.rows();o['requested_duration_game_minutes']=duration
                self.assertIsNone(CockpitRunChannel._stopped_wait_operation(o,p,q,r,w))

if __name__=='__main__':unittest.main()
