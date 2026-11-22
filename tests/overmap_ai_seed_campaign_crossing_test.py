#!/usr/bin/env python3
"""A crossing needs both native assertions and run-bound member events."""
import unittest
import overmap_ai_seed_campaign as campaign


OUTPUT = '''
x: PASSED: outing.member_ids == party_ids for: { 2, 3 } == { 2, 3 }
x: PASSED: structural_admission.staged.size() == 1 for: 1 == 1
x: PASSED: structural_admission.staged.front().npc_id == first->getID() for: 2 == 2
x: PASSED: lease.owner == "admission" for: "admission" == "admission" with message: 'minute=730 phase=outbound owner=abstract epoch=0 native id=2 at=(3000,3440,0)'
x: PASSED: structural_admission_lease_matches( camp, id for: true
x: PASSED: !(first->get_bandit_live_world_projection_lease().present) for: !false
x: PASSED: first->pos_abs() == departure_position for: (3000,3440,0) == (3000,3440,0)
x: PASSED: serviced_active_turns == journey.site().active_outing.activation.active_turns for: 600 (0x258) == 600 (0x258)
x: PASSED: first->pos_abs() != departure_position for: (2998,3444,0) != (3000,3440,0)
'''
EVENTS = [
    {'transition': 'active_sortie_dispatch', 'actor_ids': [2, 3], 'game_minutes': 720},
    {'transition': 'scout_travel_prepare', 'actor_ids': [2], 'game_minutes': 720},
    {'transition': 'scout_travel_prepare', 'actor_ids': [3], 'game_minutes': 720},
    {'transition': 'scout_motor_service', 'actor_ids': [3], 'game_minutes': 735, 'outcome': 'shared_travel_committed'},
    {'transition': 'scout_motor_service', 'actor_ids': [2], 'game_minutes': 745, 'outcome': 'shared_travel_committed'},
]


class CrossingObservation(unittest.TestCase):
    def test_requires_both_original_members_and_real_resume(self):
        result = campaign.crossing_observation(OUTPUT, EVENTS)
        self.assertEqual(result['member_ids'], [2, 3])
        self.assertEqual(result['serviced_active_turns'], 600)
        self.assertEqual(result['resume_to'], '(2998,3444,0)')
        with self.assertRaisesRegex(ValueError, 'member movement missing'):
            campaign.crossing_observation(OUTPUT, EVENTS[:-1])
        with self.assertRaisesRegex(ValueError, 'staged member'):
            campaign.crossing_observation(OUTPUT.replace('for: 2 == 2', 'for: 3 == 3'), EVENTS)


if __name__ == '__main__':
    unittest.main()
