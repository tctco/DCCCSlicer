#!/usr/bin/env python3
"""Subset official Adobe Source Han fonts for the current website copy.

Requires fonttools and brotli. Full source fonts stay in ignored local cache;
only renamed WOFF2 subsets and their OFL licenses belong in website/fonts/.
"""
from pathlib import Path
import argparse
import shutil
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path, default=ROOT / '.cache/website-fonts')
    args = parser.parse_args()
    website = ROOT / 'website'
    characters = ''.join(sorted(set(''.join((website / name).read_text() for name in ['index.html', 'zh/index.html', 'ui.js']))))
    output = website / 'fonts'
    output.mkdir(exist_ok=True)
    specs = [
        ('source-han-serif-cn-light.otf', 'DCCC Song', 'Light', 'DCCCSong-Light', 'source-han-serif-light.woff2', 'source-han-serif-LICENSE.txt'),
        ('source-han-sans-cn-regular.otf', 'DCCC Hei', 'Regular', 'DCCCHei-Regular', 'source-han-sans-regular.woff2', 'source-han-sans-LICENSE.txt'),
    ]
    for source, family, style, postscript, filename, license_name in specs:
        font = TTFont(args.source_dir / source)
        print(source, 'native weight:', font['OS/2'].usWeightClass)
        options = subset.Options()
        options.layout_features = ['*']
        options.name_IDs = ['*']
        options.name_languages = ['*']
        worker = subset.Subsetter(options=options)
        worker.populate(text=characters)
        worker.subset(font)
        # Rename modified subsets; retain the source copyright/license records.
        names = {1: family, 2: style, 3: postscript + '-website-subset', 4: family + ' ' + style, 6: postscript, 16: family, 17: style}
        for record in font['name'].names:
            if record.nameID in names:
                record.string = names[record.nameID].encode(record.getEncoding())
        if 'CFF ' in font:
            cff = font['CFF '].cff
            cff.fontNames = [postscript]
            cff.topDictIndex[0].FullName = family + ' ' + style
            cff.topDictIndex[0].FamilyName = family
        font.flavor = 'woff2'
        font.save(output / filename)
        shutil.copyfile(args.source_dir / license_name, output / license_name)
        print(filename, (output / filename).stat().st_size, 'bytes')


if __name__ == '__main__':
    main()
