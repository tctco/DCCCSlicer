'use strict';
const { t, setTheme } = window.DCCCUI;
const copyButton = document.querySelector('#copy-install');
const copyStatus = document.querySelector('#copy-status');
let copyResetTimer;

// Clipboard API is unavailable on the Windows-to-WSL HTTP preview URL.
// Execute the fallback during the click gesture so browsers can copy normally.
function copyWithSelection(text) {
  const focused = document.activeElement;
  const selection = window.getSelection();
  const ranges = selection ? Array.from({ length: selection.rangeCount }, (_, i) => selection.getRangeAt(i).cloneRange()) : [];
  const input = document.createElement('textarea');
  input.value = text;
  input.readOnly = true;
  input.tabIndex = -1;
  input.style.cssText = 'position:fixed;left:-9999px;top:0;opacity:0;font-size:16px';
  document.body.append(input);
  try {
    input.focus({ preventScroll: true });
    input.select();
    input.setSelectionRange(0, input.value.length);
    return document.execCommand('copy');
  } catch {
    return false;
  } finally {
    input.remove();
    focused?.focus({ preventScroll: true });
    if (selection) {
      selection.removeAllRanges();
      for (const range of ranges) selection.addRange(range);
    }
  }
}

copyButton?.addEventListener('click', async () => {
  clearTimeout(copyResetTimer);
  const command = document.querySelector('.install code');
  const text = command.textContent.trim();
  let copied = false;
  if (window.isSecureContext && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(text);
      copied = true;
    } catch {
      copied = copyWithSelection(text);
    }
  } else {
    copied = copyWithSelection(text);
  }
  if (copied) {
    copyButton.textContent = t('copied');
    copyButton.removeAttribute('title');
    copyStatus.textContent = t('copySuccess');
    copyResetTimer = setTimeout(() => { copyButton.textContent = t('copy'); }, 2000);
  } else {
    // Both browser copy methods can be blocked by policy; keep feedback honest.
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents(command);
    selection?.removeAllRanges();
    selection?.addRange(range);
    copyButton.textContent = t('copyFallback');
    copyButton.title = t('copyManual');
    copyStatus.textContent = t('copyManual');
  }
});

// Keep detailed provenance available without extending the desktop page.
const method = document.querySelector('#projection-method');
document.querySelectorAll('[data-open-method]').forEach(button => {
  button.addEventListener('click', () => method.showModal());
});
document.querySelector('#close-method')?.addEventListener('click', () => method.close());
method?.addEventListener('click', event => {
  if (event.target !== method) return;
  const bounds = method.getBoundingClientRect();
  if (event.clientX < bounds.left || event.clientX > bounds.right ||
      event.clientY < bounds.top || event.clientY > bounds.bottom) method.close();
});

const themeToggle = document.querySelector('#theme-toggle');
function updateThemeButton() {
  const dark = document.documentElement.dataset.theme === 'dark';
  const label = t(dark ? 'switchLight' : 'switchDark');
  themeToggle.setAttribute('aria-label', label);
  themeToggle.setAttribute('aria-pressed', String(dark));
  themeToggle.title = label;
}
themeToggle.addEventListener('click', () => {
  setTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark');
});
document.addEventListener('dccc:themechange', updateThemeButton);
updateThemeButton();

// Desktop presentation navigation; mobile keeps the readable stacked layout.
(() => {
  const slides = [document.querySelector('.home-screen'), document.querySelector('.thanks-screen')];
  if (slides.some(slide => !slide)) return;
  const desktop = matchMedia('(min-width:1000px)');
  const dots = [...document.querySelectorAll('[data-slide]')];
  let current = location.hash === '#thanks' ? 1 : 0;
  let lastChange = 0;
  let wheelSum = 0;
  let wheelTimer;
  function show(index, updateHash = true) {
    current = Math.max(0, Math.min(1, index));
    document.body.dataset.slide = String(current);
    slides.forEach((slide, i) => {
      slide.inert = desktop.matches && i !== current;
      if (desktop.matches) slide.setAttribute('aria-hidden', String(i !== current));
      else slide.removeAttribute('aria-hidden');
    });
    dots.forEach((dot, i) => {
      if (i === current) dot.setAttribute('aria-current', 'page');
      else dot.removeAttribute('aria-current');
    });
    if (updateHash && desktop.matches) history.replaceState(null, '', current ? '#thanks' : location.pathname + location.search);
  }
  dots.forEach((dot, i) => dot.addEventListener('click', () => show(i)));
  document.querySelectorAll('a[href="#thanks"],a[href="#top"]').forEach(link => link.addEventListener('click', event => {
    if (!desktop.matches) return;
    event.preventDefault();
    show(link.hash === '#thanks' ? 1 : 0);
  }));
  window.addEventListener('wheel', event => {
    if (!desktop.matches || document.querySelector('dialog[open]') || event.ctrlKey) return;
    event.preventDefault();
    clearTimeout(wheelTimer);
    wheelTimer = setTimeout(() => { wheelSum = 0; }, 160);
    if (performance.now() - lastChange < 950) return;
    wheelSum += event.deltaY;
    if (Math.abs(wheelSum) < 35) return;
    const next = current + Math.sign(wheelSum);
    wheelSum = 0;
    if (next < 0 || next > 1) return;
    lastChange = performance.now();
    show(next);
  }, { passive: false });
  window.addEventListener('keydown', event => {
    if (!desktop.matches || document.querySelector('dialog[open]') || event.target.closest('input,textarea,select,button,canvas,[contenteditable]')) return;
    const direction = ['ArrowDown', 'PageDown', ' '].includes(event.key) ? 1 : ['ArrowUp', 'PageUp'].includes(event.key) ? -1 : 0;
    if (direction || ['Home', 'End'].includes(event.key)) {
      event.preventDefault();
      show(event.key === 'Home' ? 0 : event.key === 'End' ? 1 : current + direction);
    }
  });
  function fit() {
    const content = slides[1].querySelector('.ack-main');
    content.style.transform = '';
    if (desktop.matches) content.style.transform = `scale(${Math.min(1, innerHeight / content.scrollHeight)})`;
  }
  desktop.addEventListener('change', () => { show(current, false); fit(); });
  window.addEventListener('resize', fit);
  window.addEventListener('hashchange', () => show(location.hash === '#thanks' ? 1 : 0, false));
  document.fonts.ready.then(fit);
  new ResizeObserver(fit).observe(slides[1].querySelector('.ack-main'));
  show(current, false);
})();
