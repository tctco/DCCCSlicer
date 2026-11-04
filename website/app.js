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

copyButton.addEventListener('click', async () => {
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
document.querySelector('#close-method').addEventListener('click', () => method.close());
method.addEventListener('click', event => {
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
