// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
(() => {
  if (typeof HTMLDialogElement === 'undefined') return;
  const dialog = document.createElement('dialog');
  dialog.className = 'image-dialog';
  const close = document.createElement('button');
  close.type = 'button';
  close.textContent = '×';
  close.className = 'image-dialog-close';
  close.setAttribute('aria-label', document.querySelector('meta[name="fs-close-image"]').content);
  const image = document.createElement('img');
  image.addEventListener('load', () => {
    image.style.setProperty('--image-aspect-ratio', image.naturalWidth / image.naturalHeight);
    dialog.classList.toggle('image-dialog-small', image.naturalWidth < 480);
  });
  dialog.append(close, image);
  document.body.append(dialog);
  let opener;
  document.addEventListener('click', event => {
    const link = event.target.closest('a.screenshot-link');
    if (!link || event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    opener = link;
    image.src = link.href;
    image.alt = link.querySelector('img').alt;
    dialog.setAttribute('aria-label', image.alt);
    dialog.showModal();
    close.focus();
  });
  close.addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', event => {
    if (event.target === dialog) dialog.close();
  });
  dialog.addEventListener('close', () => {
    image.removeAttribute('src');
    opener?.focus();
  });
})();
