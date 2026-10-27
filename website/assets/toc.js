function selectHeading() {
  const id = decodeURIComponent(location.hash.slice(1)) || 'overview';
  const links = document.querySelectorAll('#site-nav a');
  links.forEach(link => link.classList.remove('active'));
  const link = [...links].find(item => item.getAttribute('href') === '#' + id);
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
document.addEventListener('DOMContentLoaded', selectHeading);

document.addEventListener('click', event => {
  if (event.target.closest('a[href="#top"]')) {
    document.querySelector('.main')?.scrollTo({top: 0});
  }
});
