#!/usr/bin/env python3
"""Generate a crawlable Chinese page from the shared English homepage.

No runtime translation dependency or duplicated 3D assets. Run after editing
English content; --check verifies that committed generated pages are current.
"""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[1] / 'website'
TRANSLATIONS = {
    'Skip to content': '跳转到正文',
    'DCCC home': 'DCCC 首页',
    'Open science, one scan at a time.': '脑 PET 空间标准化与定量分析',
    'DEEP CASCADED CEREBRAL CALCULATOR': '深度级联脑 PET 定量工具',
    'A clearer picture<br>of <span class="underlined">brain PET.</span>': '脑 PET<br><span class="underlined">定量工具</span>',
    'For clinicians, researchers,<br>and <strong>AI agents.</strong>': '面向临床医生、研究人员<br>和 <strong>AI 智能体</strong>。',
    'MRI-free <span>·</span>': '无需 MRI <span>·</span>',
    'THREE TOOLS. YOUR WAY IN.': '三种使用方式',
    '>VISUALIZE<': '>交互分析<',
    '>AUTOMATE<': '>自动处理<',
    '>PYTHON ↗<': '>Python 接口 ↗<',
    'Explore and quantify brain PET in 3D Slicer.': '在 3D Slicer 中查看影像、计算指标并检查配准。',
    'Automate PET normalization and quantification.': '自动完成 PET 空间标准化与定量分析。',
    'Python tools for PET neuroimaging research.': '通过 Python 调用 PET 处理与定量功能。',
    'Download extension ↗': '下载扩展 ↗',
    'Download core ↗': '下载核心 ↗',
    'CLI guide ↗': '命令行文档 ↗',
    'Read the Python API guide': '阅读 Python 接口文档',
    'Copy pip install command': '复制 pip 安装命令',
    '>Copy</button>': '>复制</button>',
    'less preparation, more exploration.': '仅需 PET，无需配套 MRI。',
    'Colin27 cortical surface with PET uptake. Drag or use arrow keys to rotate. Use the slider below to explore interpolated uptake.': '显示 PET 摄取的 Colin27 皮层表面。拖动或使用方向键旋转，通过下方滑条查看连续插值效果。',
    'Loading brain…': '正在加载脑图…',
    'Brain view': '脑图视角',
    'Pause brain rotation': '暂停脑图旋转',
    'Left hemisphere view': '左侧视角',
    'Superior view': '顶部视角',
    'Right hemisphere view': '右侧视角',
    'Explore amyloid uptake': '淀粉样蛋白摄取',
    'Play uptake transition': '播放沉积变化',
    'Illustrative PET uptake, interpolated between different participants. Not measured scans or disease progression. Details are available in About the illustration.': '基于不同参与者 PET 的插值示意，不代表实际扫描或疾病进展。详见“关于脑图”。',
    'THE SCIENCE BEHIND IT': '相关论文',
    'Built on research.<br>Open for discovery.': '相关论文',
    'Spatial normalization & quantification': '空间标准化与定量分析',
    'Interpretable pathology decomposition': '可解释的病理分解',
    'Citation details & BibTeX ↗': '引用信息与 BibTeX ↗',
    'Research use only · Not for clinical practice': '仅用于科研 · 不用于临床诊疗',
    'CC BY-NC 4.0 & publication terms ↗': 'CC BY-NC 4.0 与发表条款 ↗',
    'About the illustration': '关于脑图',
    'BEHIND THE ILLUSTRATION': '脑图说明',
    'Real PET. A shared surface.': '数据来源与显示方法',
    'Close illustration details': '关闭脑图说明',
    'Eleven native-space PET images were normalized with DCCC, referenced to whole-cerebellum uptake, and projected onto the Colin27 pial surface. An affine T1-to-T1 registration aligns the template spaces. Cortical MIP takes the maximum of 12 samples along each inward surface normal, 0.5–6 mm beneath the pial surface, restricted to gray-matter-supported locations.': '11 个原生空间 PET 经 DCCC 标准化，以全小脑摄取为参考，投影到 Colin27 软脑膜表面。通过 T1 图像间的仿射配准对齐模板空间；沿表面法线向内 0.5–6 mm 采样，在有灰质支持的位置取 12 个采样值中的最大值，生成皮层 MIP。',
    'The continuous slider uses a simplified illustrative 0–100 CL scale, blending the PET examples selected for nominal 10-CL intervals. Example values are approximate; the actual supplied CL values remain in the provenance. Intermediate frames are visual interpolations, not measured scans. These are different participants, not disease progression. Low uptake is transparent over the neutral anatomy. A red pigment layer appears smoothly between 1.1 and 1.75 SUVr. These are display settings, not clinical thresholds or a validated cross-tracer calibration for PiB and AV45.': '滑条使用简化的 0–100 CL 示意刻度，在按约 10 CL 间隔选取的 PET 示例之间连续插值。实际 CL 值保留在来源记录中；中间帧是视觉插值，不是实际扫描。病例来自不同参与者，不表示疾病进展。低摄取层透明，红色在 1.1–1.75 SUVr 之间逐渐显现。这些数值是显示参数，并非临床阈值，也不是经验证的 PiB 与 AV45 跨示踪剂标定。',
    'Source CL values are retained without recalculation, including 82.82 and 112.57 CL for the nominal 90/100 examples. This illustration used the explicitly selected local DCCCcore 4.6.1-alpha runtime. Original PET images and processing records remain local.': '保留原始提供的 CL 值，不重新计算；名义 90/100 示例的实际值为 82.82/112.57 CL。脑图处理使用显式选定的本地 DCCCcore 4.6.1-alpha。原始 PET 与处理记录保存在本地。',
    'Surface provenance ↗': '表面数据来源 ↗',
    'Colin27 anatomy ↗': 'Colin27 解剖模板 ↗',
    'Credits ↗': '来源与致谢 ↗',
}


def generate():
    source = (ROOT / 'index.html').read_text()
    head, body = source.split('<body>', 1)
    for original, translated in TRANSLATIONS.items():
        if original not in body:
            raise ValueError(f'English source changed; update Chinese translation: {original[:80]}')
        body = body.replace(original, translated)
    english_switch = '<a class="language-switch" href="zh/?lang=zh" lang="zh-CN" hreflang="zh-CN" aria-label="切换到中文">中文</a>'
    assert english_switch in body
    body = body.replace(english_switch, '<a class="language-switch" href="../?lang=en" lang="en" hreflang="en" aria-label="Switch to English">EN</a>')
    body = body.replace('aria-label="Switch to dark theme"', 'aria-label="切换深色主题"')
    head = head.replace('<html lang="en">', '<html lang="zh-CN">')
    title = 'DCCC | 脑 PET 定量与 Centiloid 神经影像分析'
    description = '面向临床医生、研究者与 AI 智能体的脑 PET 神经影像定量工具。无需 MRI，支持 Centiloid、CenTauR 与 SUVr，提供 DCCCSlicer、DCCCcore 和 dcccpy。'
    head = re.sub(r'<title>.*?</title>', f'<title>{title}</title>', head)
    head = re.sub(r'(<meta (?:name|property)="(?:description|og:description|twitter:description)" content=")[^"]*(">)', lambda m: m[1] + description + m[2], head)
    head = re.sub(r'(<meta (?:name|property)="(?:og:title|twitter:title)" content=")[^"]*(">)', lambda m: m[1] + title + m[2], head)
    head = head.replace('<link rel="canonical" href="https://brain.pet/">', '<link rel="canonical" href="https://brain.pet/zh/">')
    head = head.replace('<meta property="og:url" content="https://brain.pet/">', '<meta property="og:url" content="https://brain.pet/zh/">')
    head = head.replace('content="en_US"', 'content="zh_CN"')
    head = re.sub(r'(<meta (?:name|property)="(?:og:image:alt|twitter:image:alt)" content=")[^"]*(">)', lambda m: m[1] + 'DCCC 脑 PET 定量工具：皮层表面的红色 PET 摄取示意。' + m[2], head)
    match = re.search(r'<script type="application/ld\+json">\s*(.*?)\s*</script>', head, re.S)
    data = json.loads(match[1])
    website, page, software = data['@graph']
    website['inLanguage'] = ['en', 'zh-CN']
    page.update({'@id': 'https://brain.pet/zh/#webpage', 'url': 'https://brain.pet/zh/', 'name': title, 'description': description, 'inLanguage': 'zh-CN'})
    software['description'] = description
    head = head[:match.start()] + '<script type="application/ld+json">\n' + json.dumps(data, ensure_ascii=False, indent=2) + '\n  </script>' + head[match.end():]
    result = head + '<body>' + body
    for name in ['favicon.svg', 'style.css', 'ui.js', 'app.js', 'pet-viewer.js', 'sitemap.xml']:
        result = result.replace(f'="{name}"', f'="../{name}"')
    result = result.replace('href="assets/', 'href="../assets/')
    sitemap = '''<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">
'''
    for url in ['https://brain.pet/', 'https://brain.pet/zh/']:
        sitemap += f'''  <url>
    <loc>{url}</loc>
    <xhtml:link rel="alternate" hreflang="en" href="https://brain.pet/" />
    <xhtml:link rel="alternate" hreflang="zh-CN" href="https://brain.pet/zh/" />
    <xhtml:link rel="alternate" hreflang="x-default" href="https://brain.pet/" />
  </url>
'''
    sitemap += '</urlset>\n'
    return {ROOT / 'zh/index.html': result, ROOT / 'sitemap.xml': sitemap}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for path, content in generate().items():
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit(f'Outdated generated page: {path}. Run {Path(__file__).name}.')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
    print('Localized pages are current.' if args.check else 'Generated Chinese homepage and multilingual sitemap.')


if __name__ == '__main__':
    main()
