"""Build bilingual acknowledgements from reviewed, source-linked records."""
from pathlib import Path
from html import escape as e
import json
import re
ROOT = Path(__file__).resolve().parents[1] / 'website'

def build_pages(english, chinese):
    data = json.loads((ROOT / 'data/acknowledgements.json').read_text())
    pages = {}
    for lang, home in [('en', english), ('zh', chinese)]:
        zh = lang == 'zh'
        pick = lambda en, cn: cn if zh else en
        prefix = '../../' if zh else '../'
        route = 'zh/acknowledgements/' if zh else 'acknowledgements/'
        title = pick('Acknowledgements', '致谢')
        header = re.search(r'<header class="masthead">.*?</header>', home, re.S)[0]
        header = header.replace('href="#"', 'href="../"')
        header = re.sub(r'(<a class="language-switch" href=")[^"]*', lambda m: m[1] + ('../../acknowledgements/?lang=en' if zh else '../zh/acknowledgements/?lang=zh'), header)
        def refs(ids):
            return '<span class="sources">' + ' · '.join(f'<a href="{e(data["sources"][s]["url"])}">{e(data["sources"][s]["label"][lang])} ↗</a>' for s in ids) + '</span>'
        def contributions(person):
            return '<ul>' + ''.join(f'<li>{e(c["text"][lang])} {refs(c["sources"])}</li>' for c in person['contributions']) + '</ul>'
        def logo(item):
            if item.get('logo'):
                return f'<div class="logo-plate logo-{Path(item["logo"]).stem}"><img src="{prefix}{e(item["logo"])}" alt="" loading="lazy"></div>'
            return ''
        cards = ''
        for key, institution in data['institutions'].items():
            people = [p for p in data['contributors'] if p['institution'] == key]
            cards += f'<article class="institution">{logo(institution)}<div class="institution-heading"><p class="city">{e(institution["city"][lang])}</p><h2><a href="{e(institution["url"])}">{e(institution["name"][lang])} ↗</a></h2></div>'
            cards += ''.join(f'<div class="person"><h3>{e(p["name"])}</h3>{contributions(p)}</div>' for p in people) + '</article>'
        community = ''.join(f'<article class="community-person"><h3><a href="{e(p["url"])}">{e(p["name"])} ↗</a></h3>{contributions(p)}</article>' for p in data['community'])
        resources = ''.join(f'<article class="resource">{logo(r)}<h3><a href="{e(r["url"])}">{e(r["name"])} ↗</a></h3><p>{e(r["title"][lang])}</p><p>{e(r["description"][lang])}</p><a class="sources" href="{e(r["creditUrl"])}">{pick("Study team & credits", "研究团队与致谢")} ↗</a></article>' for r in data['resources'])
        description = pick('Thanks to the clinicians, researchers and community members who contributed data, testing and feedback to DCCC.', '感谢为 DCCC 提供数据、参与测试和反馈问题的临床医生、研究人员与社区用户。')
        html = f'''<!doctype html>
<html lang="{pick('en','zh-CN')}" data-zh-path="../zh/acknowledgements/">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} | DCCC</title><meta name="description" content="{description}"><meta name="theme-color" content="#f5f1e8">
<link rel="canonical" href="https://brain.pet/{route}">
<link rel="alternate" hreflang="en" href="https://brain.pet/acknowledgements/">
<link rel="alternate" hreflang="zh-CN" href="https://brain.pet/zh/acknowledgements/">
<link rel="alternate" hreflang="x-default" href="https://brain.pet/acknowledgements/">
<link rel="icon" href="{prefix}favicon.svg" type="image/svg+xml">
<script src="{prefix}ui.js"></script><link rel="stylesheet" href="{prefix}style.css"><link rel="stylesheet" href="{prefix}acknowledgements.css"><script src="{prefix}app.js" defer></script>
</head><body class="acknowledgements-page"><a class="skip-link" href="#main">{pick('Skip to content','跳转到正文')}</a>
{header}
<main id="main" class="ack-main"><div class="ack-intro"><a class="back-home" href="../">← {pick('Back to DCCC','返回首页')}</a><p class="eyebrow">DCCC / {pick('ACKNOWLEDGEMENTS','致谢')}</p><h1>{title}<span class="brand-dot">.</span></h1><p class="ack-lead">{description}</p></div>
<section aria-label="{pick('Contributors and affiliations','贡献者与单位')}" class="institution-grid">{cards}</section>
<section class="ack-section"><h2>{pick('From the community','社区反馈')}</h2><div class="community-grid">{community}</div></section>
<section class="ack-section"><h2>{pick('Research data','研究数据')}</h2><div class="resource-grid">{resources}</div></section>

</main><footer class="ack-footer"><a href="../">DCCC</a><span>{pick('Thank you for helping improve brain PET tools.','感谢各位对脑 PET 工具的支持。')}</span><a href="https://github.com/tctco/DCCCSlicer">GitHub ↗</a></footer></body></html>\n'''
        pages[ROOT / route / 'index.html'] = html
    return pages
