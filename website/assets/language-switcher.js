function updateLanguageLinks() {
  document.querySelectorAll('[data-language-href]').forEach(link => {
    link.href = link.dataset.languageHref + location.hash;
  });
}
document.addEventListener('DOMContentLoaded', () => {
  updateLanguageLinks();
  const picker = document.querySelector('.language-picker');
  const noResults = document.querySelector('meta[name="fs-no-results"]')?.content;
  const results = document.getElementById('search-results');
  if (noResults && results) {
    new MutationObserver(() => {
      const message = results.querySelector('.search-no-result');
      if (message && message.textContent !== noResults) message.textContent = noResults;
    }).observe(results, {childList: true, subtree: true});
  }
  if (!picker) return;
  document.addEventListener('click', event => {
    if (!picker.contains(event.target)) picker.open = false;
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && picker.open) {
      picker.open = false;
      picker.querySelector('summary').focus();
    }
  });
});
window.addEventListener('hashchange', updateLanguageLinks);
