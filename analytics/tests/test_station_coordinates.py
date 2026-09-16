from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from seed_station_coordinates import display_coordinate, district_from


class StationCoordinateTest(unittest.TestCase):
    def test_coordinates_are_deterministic_and_near_address_anchor(self):
        first = display_coordinate("129465", "高新区科学大道·交直流充电站1号", "河南省郑州市高新区科学大道")
        second = display_coordinate("129465", "高新区科学大道·交直流充电站1号", "河南省郑州市高新区科学大道")
        self.assertEqual(first, second)
        self.assertLess(abs(first[0] - 34.8100), 0.01)
        self.assertLess(abs(first[1] - 113.5760), 0.01)

    def test_unknown_address_is_rejected(self):
        with self.assertRaises(ValueError):
            display_coordinate("1", "未知站", "未知地址")

    def test_district_is_derived_from_teacher_address(self):
        self.assertEqual(district_from("河南省郑州市郑东新区金水东路"), "郑东新区")


if __name__ == "__main__":
    unittest.main()
