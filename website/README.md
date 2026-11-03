# DCCC project website

An English, responsive, buildless project homepage. The hero uses real PET uptake projected onto a Colin27 cortical surface, rendered locally with Three.js. Eleven supplied PET cases anchor a continuous visual interpolation. Desktop uses a single screen with three tool entry points, a central brain illustration and the two research papers; mobile stacks these sections vertically. Intermediate frames are illustrative, not measured scans or longitudinal disease progression. Google Fonts is optional and has system font fallbacks.

## Preview

From the repository root:

```sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory website
```

Open http://localhost:8000. No npm install or build step is required.

When previewing from the Windows host of WSL and localhost forwarding is unavailable, start the server with `--bind 0.0.0.0` instead of `--bind 127.0.0.1`. Run `hostname -I` inside WSL and open `http://<WSL-IP>:8000` in Windows. The WSL IP may change after a restart. This listener is reachable through the WSL network interface; use it for the local preview session. Use the local HTTP server: the 3D viewer loads binary assets using fetch, so opening index.html directly from disk is not supported. Copying the install command requires localhost or HTTPS.

## GitHub Pages

Source lives on the `website` branch, in `website/`. The workflow at `.github/workflows/pages.yml` uploads only that directory and does not invoke the native software release workflow.

One-time repository setup:

1. Open **Settings → Pages → Build and deployment → Source** and select **GitHub Actions**.
2. If the `github-pages` environment has branch restrictions, permit `website` under **Settings → Environments → github-pages**.
3. Push changes to `website` on GitHub. The initial push and subsequent changes to `website/` or the workflow trigger deployment. To redeploy unchanged content, use **Actions → Publish DCCC website → Run workflow**, selecting the `website` branch.

Default URL: https://tctco.github.io/DCCCSlicer/ (the deployment output is authoritative).

GitHub's workflow documentation: https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

## Editing

- `index.html`: English content and download/documentation links.
- `style.css`: paper, ink, typography and responsive layouts.
- `app.js`: install-command copy button.
- `pet-viewer.js`: local WebGL renderer, continuous surface interpolation, playback and rotation.
- `assets/pet/`: surface geometry, projected SUVr, provenance and credits (no native PET volumes).
- `vendor/`: Three.js 0.176.0 with its MIT license.
- `favicon.svg`: original brain icon.

Content is based on the repository README and CLI/Python documentation. Download links point to the releases listing, including prereleases; no software version is hard-coded. Research-only and publication restrictions are linked in the footer. Documentation links currently target the software's `dev` branch.

## Reproduce the PET projection

Input: the host desktop archive `internal_Centiloid_0_to_100_step10_native_PET_11cases.zip` and the reference project's Brainstorm/Cedalion Colin27 template. Both inputs are external to this repository. Intermediate NIfTI images, original case metadata, normalization logs and processing receipts stay in `.cache/website-pet/`, which Git ignores.

Install Python packages into a separate environment: numpy, scipy, nibabel, trimesh, fast-simplification, SimpleITK, matplotlib. Use the repository dcccpy via `PYTHONPATH=python/dcccpy/src` and explicitly select the native runtime:

```sh
PYTHONPATH=python/dcccpy/src /path/to/python scripts/prepare-website-pet.py \
  --archive /path/to/internal_Centiloid_0_to_100_step10_native_PET_11cases.zip \
  --colin /path/to/brainproject/research/functional-roi31/template/colin27 \
  --core /path/to/DCCCcore
```

This preview explicitly used the available DCCCcore **4.6.1-alpha** runtime with the repository wrapper (which expects **4.6.3-alpha**). The runtime was selected explicitly; no segmentation was performed and no software version files were changed. Normalization and template registration are recorded in `assets/pet/manifest.json`. Cached normalized images and template transforms are reused; use a fresh `--cache` directory when changing runtime, source images, or templates.

Processing: iterative DCCC normalization → whole-cerebellum SUVr → affine Colin27-T1-to-MNI152-T1 alignment → cortical MIP (maximum over 12 inward-normal samples at 0.5–6 mm, restricted to gray-matter support > 0.25) → transparent-to-red overlay over a neutral anatomical surface. Red opacity increases smoothly from 0 at 1.1 SUVr to 0.86 at 1.75 SUVr; these are visual display settings, not clinical thresholds. The high-resolution surface is decimated to 120,000 faces. The supplied actual CL values (originally calculated with runtime 4.4.0) are retained without recalculation. A shared SUVr window across PiB and AV45 is an illustration, not a validated tracer harmonization. Native PET is not distributed with the site.

The continuous slider presents a clean illustrative **0–100 CL** scale with whole-number readouts and nominal 10-CL selection targets as interpolation anchors. This intentionally approximates the actual case CL values, including **82.82** and **112.57** for the nominal 90/100 examples; exact source values remain unchanged in the provenance. Intermediate surfaces are linear blends of neighboring projected uptake arrays; the slider never interpolates anatomy or source volumes. The brain slowly rotates by default (roughly 100 seconds per turn), unless reduced motion is requested. A rotation button pauses/resumes it; choosing a view, dragging or using arrow keys pauses rotation. Rendering is capped at 30 fps and suspended while the page is hidden. Uptake playback remains opt-in. The HTML/CSS loading animation appears while local 3D assets load and is hidden when the surface is ready. Drag, arrow keys and L / superior / R controls rotate the same template anatomy.

Detailed projection provenance opens in an accessible native dialog, so it does not add height to the desktop page. Desktop layout fits viewports from 1000×560 upward; smaller windows use the flowing responsive layout.

The first-screen audience statement is “For clinicians, researchers, and AI agents.” The project's research-use and license terms remain linked in the footer.

## Custom domain: brain.pet (Namecheap)

The custom domain is managed in the repository's GitHub Pages settings (`cname` via the Pages API). This uses a custom Actions workflow, so a CNAME file in the deployed artifact is not required by GitHub.

In Namecheap: **Domain List → brain.pet → Manage → Advanced DNS → Host Records**. Use Namecheap BasicDNS if this domain's DNS is managed there. Complete any pending domain contact verification first.

| Type | Host | Value | TTL |
| --- | --- | --- | --- |
| A | @ | 185.199.108.153 | Automatic |
| A | @ | 185.199.109.153 | Automatic |
| A | @ | 185.199.110.153 | Automatic |
| A | @ | 185.199.111.153 | Automatic |
| CNAME | www | tctco.github.io | Automatic |

Replace conflicting parking/redirect records for `@` and `www`; keep unrelated mail/TXT records. Do not include `/DCCCSlicer/` in the CNAME value. After DNS validation and certificate issuance, enable **Enforce HTTPS** in GitHub Pages settings. The intended canonical URL is https://brain.pet/.

References: [GitHub custom domains](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site), [Namecheap GitHub Pages setup](https://www.namecheap.com/support/knowledgebase/article.aspx/9645/2208/how-do-i-link-my-domain-to-github-pages/).

## Search and sharing

Search metadata targets brain PET, neuroimaging, Centiloid and PET quantification. The homepage has a descriptive title/meta description, an HTTPS canonical URL, Open Graph/Twitter metadata, a 1200×630 sharing card, and server-delivered JSON-LD describing the website and software suite. Short, visible tool descriptions use the same terminology without expanding the page layout.

`robots.txt` permits crawling and points to `https://brain.pet/sitemap.xml`; the sitemap lists the canonical homepage. The keywords meta tag is descriptive only: Google does not use it for ranking. No ratings, clinical efficacy claims, or rich-result eligibility are asserted.

After HTTPS is active, the domain owner can add `brain.pet` to Google Search Console and submit the sitemap. Ownership verification and search indexing are separate from deployment. Keep canonical URLs, sharing URLs, JSON-LD and sitemap consistent when changing domains.

References: [Google-supported metadata](https://developers.google.com/search/docs/crawling-indexing/special-tags), [Google developer SEO guide](https://developers.google.com/search/docs/fundamentals/get-started-developers), [Schema.org SoftwareApplication](https://schema.org/SoftwareApplication).
