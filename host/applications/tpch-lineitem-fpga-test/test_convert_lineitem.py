import csv
import struct
import tempfile
import unittest
from pathlib import Path

import convert_lineitem as converter


ROWS = [
    "1|155190|7706|1|17|21168.23|0.06|0.02|N|O|1994-03-13|1994-02-12|1994-03-22|DELIVER IN PERSON|TRUCK|first row|\n",
    "2|67310|7311|2|36|45983.16|0.06|0.06|N|O|1994-04-12|1994-02-28|1994-04-20|TAKE BACK RETURN|MAIL|quantity too high|\n",
    "3|63700|3701|3|8|13309.60|0.10|0.02|R|F|1996-01-29|1996-03-05|1996-01-31|NONE|REG AIR|date and discount miss|\n",
]


class ConvertLineitemTest(unittest.TestCase):
    def test_parse_encode_and_q6(self):
        item = converter.parse_lineitem(ROWS[0])
        self.assertEqual(item.quantity, 17)
        self.assertEqual(item.extendedprice_cents, 2_116_823)
        self.assertEqual(item.discount_bp, 600)
        self.assertEqual(item.shipdate, 19940313)
        self.assertTrue(converter.q6_selected(item))

        record = converter.encode_record(item, 1)
        self.assertEqual(len(record), 512)
        self.assertEqual(struct.unpack_from("<Q", record, 0)[0], 1)
        self.assertEqual(record[25], 17)
        self.assertEqual(struct.unpack_from("<Q", record, 32)[0], 2_116_823)
        self.assertEqual(struct.unpack_from("<H", record, 40)[0], 600)
        self.assertEqual(struct.unpack_from("<I", record, 44)[0], 19940313)
        raw_length = struct.unpack_from("<H", record, 160)[0]
        self.assertEqual(
            record[converter.RAW_LINE_OFFSET : converter.RAW_LINE_OFFSET + raw_length].decode("ascii"),
            ROWS[0].rstrip("\n"),
        )

    def test_convert_limit_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "lineitem.tbl"
            output = root / "lineitem.bin"
            manifest = root / "lineitem.csv"
            source.write_text("".join(ROWS), encoding="ascii")

            count, selected = converter.convert(source, output, manifest, 2, 0)
            self.assertEqual(count, 2)
            self.assertEqual(selected, [17])
            self.assertEqual(len(output.read_bytes()), 2 * converter.RECORD_BYTES)
            with manifest.open(newline="", encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["q6_selected"], "1")
            self.assertEqual(rows[1]["q6_selected"], "0")

    def test_rejects_wrong_column_count(self):
        with self.assertRaisesRegex(ValueError, "expected 16"):
            converter.parse_lineitem("1|2|3|\n")


if __name__ == "__main__":
    unittest.main()

