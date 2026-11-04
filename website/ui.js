/* Apply saved preferences before styles paint; shared by both static pages. */
(() => {
  'use strict';
  const root = document.documentElement;
  const language = root.lang.startsWith('zh') ? 'zh' : 'en';
  const systemTheme = matchMedia('(prefers-color-scheme: dark)');
  const read = key => { try { return localStorage.getItem(key); } catch { return null; } };
  const write = (key, value) => { try { localStorage.setItem(key, value); } catch { /* Private mode can disable storage. */ } };
  const requested = new URLSearchParams(location.search).get('lang');
  const preferred = read('dccc-language');
  const redirectToZh = language === 'en' && (requested === 'zh' || (!requested && preferred === 'zh'));
  if (!redirectToZh) write('dccc-language', language);
  const savedTheme = read('dccc-theme');
  root.dataset.theme = ['light', 'dark'].includes(savedTheme) ? savedTheme : (systemTheme.matches ? 'dark' : 'light');
  const translations = {
    en: {
      copy: 'Copy', copied: 'Copied!', copySuccess: 'Install command copied.',
      copyManual: 'Select and copy the command: pip install dcccpy', copyFallback: 'Copy manually',
      rotationPause: 'Pause brain rotation', rotationResume: 'Resume brain rotation',
      uptakePlay: 'Play uptake transition', uptakePause: 'Pause uptake transition',
      sliderValue: '{value} CL on the illustrative scale, visually blended PET examples',
      interrupted: '3D view interrupted. Reload to restore the surface.',
      localPreview: 'Open this preview at http://localhost:8000 to load the 3D surface.',
      loadError: 'The 3D surface could not load. Try reloading or enabling WebGL.',
      switchDark: 'Switch to dark theme', switchLight: 'Switch to light theme'
    },
    zh: {
      copy: '复制', copied: '已复制', copySuccess: '安装命令已复制。',
      copyManual: '请选中并复制命令：pip install dcccpy', copyFallback: '手动复制',
      rotationPause: '暂停脑图旋转', rotationResume: '继续脑图旋转',
      uptakePlay: '播放沉积变化', uptakePause: '暂停沉积变化',
      sliderValue: '示意刻度 {value} CL，连续混合的 PET 示例',
      interrupted: '三维显示已中断，请刷新页面恢复。',
      localPreview: '请通过 http://localhost:8000 打开预览，以加载三维脑图。',
      loadError: '三维脑图加载失败，请刷新页面或启用 WebGL。',
      switchDark: '切换深色主题', switchLight: '切换浅色主题'
    }
  };
  const t = (key, values = {}) => (translations[language][key] || translations.en[key] || key)
    .replace(/\{(\w+)\}/g, (_, name) => values[name] ?? '');
  const setTheme = (theme, persist = true) => {
    root.dataset.theme = theme;
    if (persist) write('dccc-theme', theme);
    const color = theme === 'dark' ? '#171b19' : '#f5f1e8';
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', color);
    document.dispatchEvent(new CustomEvent('dccc:themechange', { detail: { theme } }));
  };
  systemTheme.addEventListener('change', event => {
    if (!['light', 'dark'].includes(read('dccc-theme'))) setTheme(event.matches ? 'dark' : 'light', false);
  });
  window.DCCCUI = { language, t, setTheme };
  document.querySelector('meta[name="theme-color"]')?.setAttribute('content', root.dataset.theme === 'dark' ? '#171b19' : '#f5f1e8');
  if (redirectToZh) location.replace(new URL('zh/', location.href));
})();
