function selectHeading() {
  const requested = decodeURIComponent(location.hash.slice(1)) || 'overview';
  const id = document.getElementById(requested)?.dataset.aliasFor || requested;
  const links = [...document.querySelectorAll('#site-nav a[href^="#"]')];
  links.forEach(link => link.classList.remove('active'));
  const target = document.getElementById(id);
  const link = links.find(item => item.getAttribute('href') === '#' + id)
    || (target && links.findLast(item => {
      const heading = document.getElementById(decodeURIComponent(item.hash.slice(1)));
      return heading && (heading.compareDocumentPosition(target) & Node.DOCUMENT_POSITION_FOLLOWING);
    }));
  if (!link) return;
  link.classList.add('active');
  let parent = link.parentElement;
  while (parent && parent.id !== 'site-nav') {
    if (parent.classList.contains('nav-list-item')) {
      parent.classList.add('active');
      parent.querySelector(':scope > button')?.setAttribute('aria-expanded', 'true');
    }
    parent = parent.parentElement;
  }
}
window.addEventListener('hashchange', selectHeading);
document.addEventListener('DOMContentLoaded', () => {
  selectHeading();
  document.getElementById('back-to-top')?.addEventListener('click', () => {
    document.querySelector('.main-content-wrap')?.scrollTo({top: 0});
  });
});
