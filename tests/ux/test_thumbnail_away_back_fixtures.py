"""Bounded LIB005 infrastructure; owned small files and mocked child, no UI/build."""
import hashlib
from pathlib import Path
import tempfile
import unittest

import export_watermark_fixtures as watermark
import quality_policy as policy
import run_ux_suite as runner
import test_thumbnail_response_fixtures as response_tests
import thumbnail_response_fixtures as palette


class ThumbnailAwayBackFixtureTests(unittest.TestCase):
    run_cli = response_tests.ThumbnailFixtureTests.run_cli

    def test_distinct_fresh_profile_has_exact_same32_rgb_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            folder = root/'case-fixtures'/palette.AWAY_BACK_CASE
            hashes, manifest = palette.official_fixture(folder, owned_root=root, case_id=palette.AWAY_BACK_CASE)
            other = root/'case-fixtures'/palette.OFFICIAL_CASE
            other_hashes, _ = palette.official_fixture(other, owned_root=root)
            self.assertEqual(sorted(Path(path).name for path in hashes), [f'Photo-{i:04}.png' for i in range(32)])
            self.assertEqual(manifest['case_id'], 'UX-LIB-005')
            self.assertEqual(manifest['name'], 'thumbnail-away-back-rgb32-v1')
            self.assertEqual((manifest['public_photo_count'], manifest['original_checksum_count']), (32, 32))
            self.assertEqual((manifest['width'], manifest['height'], manifest['png_color_type']), (256, 192, 'RGB'))
            self.assertFalse(manifest['large_24mp_generated'])
            for name, digest in hashes.items():
                path = Path(name)
                self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest())
                self.assertEqual(path.read_bytes(), (other/path.name).read_bytes())
                self.assertFalse(path.is_symlink())
            self.assertFalse(set(hashes).intersection(other_hashes))
            self.assertEqual(len(runner.merge_fixture_hashes([(folder, hashes), (other, other_hashes)])), 64)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder, owned_root=root, case_id=palette.AWAY_BACK_CASE)
            self.assertEqual(runner.changed_originals(hashes), [])

    def test_solo_both_modes_use_only32_once_for_three_children(self):
        for quality in (False, True):
            with self.subTest(quality=quality):
                outcome = self.run_cli([palette.AWAY_BACK_CASE], quality=quality, runs=3)
                self.assertEqual((outcome['code'], outcome['base_calls'], outcome['extra_calls']), (0, 0, 1))
                self.assertEqual(len(outcome['calls']), 3)
                self.assertEqual(len({str(folder) for _, folder in outcome['calls']}), 1)
                self.assertTrue(all(folder.parts[-2:] == ('case-fixtures', 'UX-LIB-005') for _, folder in outcome['calls']))
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'], profile['extra_public_photo_count'], profile['original_checksum_count']), (0, 0, 32, 32))
                self.assertFalse(profile['large_24mp_generated'])
                self.assertEqual(profile, outcome['manifest']['fixture_profile'])
                self.assertEqual(len(outcome['manifest']['original_sha256']), 32)
                self.assertEqual(outcome['summary']['inventory_domain'], 'specification')
                self.assertEqual(outcome['summary']['release_gate'], 'NOT_QUALIFIED')

    def test_all_four_exact_profiles_union70_without_base(self):
        ids = [palette.OFFICIAL_CASE, palette.AWAY_BACK_CASE, palette.RACE_CASE, watermark.OFFICIAL_CASE, 'UX-UND-018']
        outcome = self.run_cli(ids, runs=3)
        self.assertEqual((outcome['code'], outcome['base_calls'], outcome['extra_calls']), (0, 0, 3))
        self.assertEqual(len(outcome['calls']), 12)
        profile = outcome['summary']['fixture_profile']
        self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'], profile['extra_public_photo_count'], profile['original_checksum_count']), (0, 0, 70, 70))
        self.assertEqual({p['case_id']:p['public_photo_count'] for p in profile['case_profiles']}, {palette.OFFICIAL_CASE:32, palette.AWAY_BACK_CASE:32, palette.RACE_CASE:2, watermark.OFFICIAL_CASE:4})
        self.assertEqual(len({p['directory'] for p in profile['case_profiles']}), 4)
        self.assertFalse(profile['large_24mp_generated'])
        self.assertEqual(len(outcome['manifest']['original_sha256']), 70)
        self.assertLess(len(set(outcome['manifest']['original_sha256'].values())), 70)
        integrity = [row for row in outcome['summary']['results'] if row['id'] == 'UX-UND-018']
        self.assertEqual(len(integrity), 3)
        self.assertTrue(all(row['original_count'] == 70 and row['status'] == 'PASS' for row in integrity))

    def test_all_profiles_keep_generic10_or20_and_two_dng_unchanged(self):
        profiles = [palette.OFFICIAL_CASE, palette.AWAY_BACK_CASE, palette.RACE_CASE, watermark.OFFICIAL_CASE]
        for identifier, count, union in [('UX-SEL-002',10,82), ('UX-IMP-009',20,92)]:
            with self.subTest(identifier=identifier):
                outcome = self.run_cli([*profiles,identifier,'UX-UND-018'])
                self.assertEqual((outcome['code'], outcome['base_calls'], outcome['extra_calls']), (0, 1, 3))
                self.assertEqual(outcome['base_options'], {'count':count,'include_large':False})
                profile = outcome['summary']['fixture_profile']
                self.assertEqual((profile['public_photo_count'], profile['base_original_checksum_count'], profile['extra_public_photo_count'], profile['original_checksum_count']), (count,count+2,70,union))
                self.assertEqual(len(outcome['manifest']['original_sha256']), union)
                folders = dict(outcome['calls'])
                self.assertEqual(folders[identifier].name, 'public-fixtures-quality')
                for case_id in profiles:
                    self.assertEqual(folders[case_id].parts[-2:], ('case-fixtures',case_id))
                    self.assertFalse(folders[case_id].is_relative_to(folders[identifier]))
                integrity = next(row for row in outcome['summary']['results'] if row['id'] == 'UX-UND-018')
                self.assertEqual((integrity['original_count'],integrity['status']), (union,'PASS'))

    def test_ineligible_requests_fail_without_factories_or_children(self):
        options = ({'native':False}, {'override_row':{'scope':'0.6'}}, {'extra_flags':('--suite','blocking')}, {'extra_flags':('--require-build-provenance',),'provenance':'FAIL'})
        settings = [dict(item,quality=quality) for item in options for quality in (False,True)] + [{'denied':True}]
        for setting in settings:
            with self.subTest(setting=setting):
                outcome = self.run_cli([palette.AWAY_BACK_CASE], **setting)
                self.assertNotEqual(outcome['code'], 0)
                self.assertEqual((outcome['base_calls'],outcome['extra_calls'],len(outcome['calls'])), (0,0,0))
                if outcome['summary'] is not None:
                    self.assertEqual(outcome['summary']['release_gate'], 'NOT_QUALIFIED')
                    self.assertFalse(any(row['status']=='PASS' for row in outcome['summary']['results']))
        outcome = self.run_cli([palette.OFFICIAL_CASE,palette.AWAY_BACK_CASE,palette.RACE_CASE,watermark.OFFICIAL_CASE], quality=False,extra_flags=('--suite','blocking'))
        self.assertNotEqual(outcome['code'],0)
        self.assertEqual((outcome['base_calls'],outcome['extra_calls'],len(outcome['calls'])), (0,0,0))

    def test_duplicate_request_once_and_mutations_fail(self):
        outcome = self.run_cli([palette.AWAY_BACK_CASE,palette.AWAY_BACK_CASE])
        self.assertEqual((outcome['code'],outcome['extra_calls'],len(outcome['calls'])),(0,1,1))
        for mutation in ('bytes','remove','symlink'):
            with self.subTest(mutation=mutation):
                outcome = self.run_cli([palette.AWAY_BACK_CASE],mutate=mutation,mutation_case=palette.AWAY_BACK_CASE)
                self.assertEqual(outcome['code'],1)
                self.assertEqual(outcome['summary']['original_hashes'],'FAIL')
                self.assertEqual(outcome['summary']['results'][0]['original_integrity'],'FAIL')
                self.assertEqual(len(outcome['summary']['results'][0]['mutated_originals']),1)

    def test_profile_rejects_foreign_alias_symlink_and_unknown_case(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            for other in [palette.OFFICIAL_CASE,palette.RACE_CASE,watermark.OFFICIAL_CASE,'foreign']:
                with self.subTest(other=other),self.assertRaises(ValueError):
                    palette.official_fixture(root/'case-fixtures'/other,owned_root=root,case_id=palette.AWAY_BACK_CASE)
            folder = root/'case-fixtures'/palette.AWAY_BACK_CASE
            folder.parent.mkdir()
            outside = root/'outside';outside.mkdir()
            folder.symlink_to(outside,target_is_directory=True)
            with self.assertRaises(ValueError):
                palette.official_fixture(folder,owned_root=root,case_id=palette.AWAY_BACK_CASE)
            self.assertEqual(list(outside.iterdir()),[])
            with self.assertRaises(ValueError):
                palette.official_fixture(root/'case-fixtures'/'unknown',owned_root=root,case_id='unknown')

    def test_unrelated_quality_case_profile_unchanged(self):
        outcome = self.run_cli(['UX-SEL-002'])
        self.assertEqual((outcome['code'],outcome['base_calls'],outcome['extra_calls']),(0,1,0))
        self.assertEqual(outcome['base_options'],{'count':10,'include_large':False})
        self.assertEqual(outcome['summary']['fixture_profile']['original_checksum_count'],12)
        self.assertEqual(outcome['summary']['fixture_profile']['case_profiles'],[])

    def test_official_inventory_policy_ci_and_narrow_artifacts(self):
        row = next(row for row in runner.INVENTORY if row['id']==palette.AWAY_BACK_CASE)
        self.assertEqual((row['scope'],row['mode'],row['severity']),('CURRENT','REGRESSION','P1'))
        self.assertIn(row['id'],runner.NATIVE)
        self.assertIsNone(policy.quality_skip_reason(row))
        for filename in ('macos.yml','release.yml'):
            workflow = (runner.ROOT/'.github/workflows'/filename).read_text()
            self.assertIn('run_ux_suite.py --build build --quality-only --case UX-LIB-005 --runs 3',workflow)
            self.assertIn('build/ux-thumbnail-away-back-report/',workflow)
            self.assertIn('build/ux-artifacts/20*/**/lib005-*.json',workflow)
            self.assertNotIn('run_lib005_candidate.py',workflow)
            self.assertNotIn('build/ux-artifacts/20*/case-fixtures/',workflow)


if __name__ == '__main__':
    unittest.main()
