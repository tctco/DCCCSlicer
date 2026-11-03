'use strict';
const copyButton = document.querySelector('#copy-install');
copyButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText('pip install dcccpy');
    copyButton.textContent = 'Copied!';
    document.querySelector('#copy-status').textContent = 'Install command copied.';
    setTimeout(() => { copyButton.textContent = 'Copy'; }, 2000);
  } catch {
    document.querySelector('#copy-status').textContent = 'Select and copy the command: pip install dcccpy';
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents(document.querySelector('.install code'));
    selection.removeAllRanges();
    selection.addRange(range);
    copyButton.textContent = 'Select & copy';
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
