"""Make review contact sheets only. This never marks pictures inspected or accepted."""
from pathlib import Path
import argparse, json
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    args = parser.parse_args()
    receipt = json.loads((ROOT / 'Saved/StadiumDetailWork' / (args.tag + '_capture.json')).read_text())
    for run in receipt['runs']:
        folder = ROOT / 'Saved/CalderWork/runs' / run['tag'] / 'Saved/HouseExteriorViews'
        paths = sorted(folder.glob('*.png'))
        assert len(paths) == len(run['frames']) and len(paths) >= 21
        sheet = Image.new('RGB', (1600, ((len(paths) + 3) // 4) * 250), (20, 20, 20))
        draw = ImageDraw.Draw(sheet)
        for i, path in enumerate(paths):
            with Image.open(path) as original:
                preview = original.copy()
            preview.thumbnail((400, 225))
            x, y = i % 4 * 400, i // 4 * 250
            sheet.paste(preview, (x, y))
            draw.text((x + 3, y + 227), path.stem, fill='white')
        output = ROOT / 'Saved/StadiumDetailWork' / (run['tag'] + '_contact.jpg')
        sheet.save(output, quality=95)
        print(output)


if __name__ == '__main__':
    main()
