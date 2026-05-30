// Přepínání bočního menu na mobilu
document.getElementById('menu-toggle')?.addEventListener('click', () => {
  document.querySelector('.sidebar').classList.toggle('open');
});
