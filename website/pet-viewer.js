import * as THREE from './vendor/three.module.min.js';
const { t } = window.DCCCUI;

const stage = document.querySelector('#pet-stage');
const canvas = document.querySelector('#pet-canvas');
const loading = document.querySelector('#pet-loading');
const loadingLabel = document.querySelector('#loading-label');
const rotateButton = document.querySelector('#rotate-pet');
const slider = document.querySelector('#burden');
const value = document.querySelector('#cl-value');
const play = document.querySelector('#play-pet');
const viewButtons = [...document.querySelectorAll('[data-view]')];
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
let viewer;

async function getFile(name, json = false) {
  const response = await fetch(new URL(`assets/pet/${name}`, import.meta.url));
  if (!response.ok) throw new Error(`Surface asset ${name}: HTTP ${response.status}`);
  return json ? response.json() : response.arrayBuffer();
}

async function initialize() {
  const manifest = await getFile('manifest.json', true);
  const buffers = await Promise.all(['positions.bin', 'normals.bin', 'indices.bin', 'cavity.bin', 'uptake.bin'].map(name => getFile(name)));
  const [positions, normals, indices, cavity, uptake] = buffers;
  const count = manifest.vertexCount;
  if (positions.byteLength !== count * 12 || normals.byteLength !== count * 12 ||
      indices.byteLength !== manifest.triangleCount * 12 || cavity.byteLength !== count ||
      uptake.byteLength !== count * manifest.cases.length * 2) throw new Error('Surface asset size mismatch');

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: 'low-power' });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.setClearColor(0xf0eddf, 0);
  const scene = new THREE.Scene();
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
  geometry.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(normals), 3));
  geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(indices), 1));
  geometry.setAttribute('cavity', new THREE.BufferAttribute(new Uint8Array(cavity), 1, true));
  const lowAttribute = new THREE.BufferAttribute(new Float32Array(count), 1);
  const highAttribute = new THREE.BufferAttribute(new Float32Array(count), 1);
  geometry.setAttribute('uptakeLow', lowAttribute);
  geometry.setAttribute('uptakeHigh', highAttribute);
  geometry.computeBoundingBox();
  const center = geometry.boundingBox.getCenter(new THREE.Vector3());
  const camera = new THREE.OrthographicCamera(-140, 140, 100, -100, 1, 1500);
  camera.up.set(0, 0, 1);
  let azimuth = 2.9, elevation = .19;
  const uniforms = {
    blend: { value: 0 },
    overlayWindow: { value: new THREE.Vector2(...manifest.displayWindowSUVr) },
    anatomyColor: { value: new THREE.Vector3(.86, .85, .78) },
    pigmentColor: { value: new THREE.Vector3(.72, .23, .18) }
  };
  const material = new THREE.ShaderMaterial({
    uniforms,
    vertexShader: `
      attribute float cavity;
      attribute float uptakeLow;
      attribute float uptakeHigh;
      uniform float blend;
      varying vec3 vNormal;
      varying vec3 vPosition;
      varying float vCavity;
      varying float vUptake;
      void main() {
        vNormal = normalize(normalMatrix * normal);
        vPosition = position;
        vCavity = cavity;
        vUptake = mix(uptakeLow, uptakeHigh, blend);
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }`,
    fragmentShader: `
      varying vec3 vNormal;
      varying vec3 vPosition;
      varying float vCavity;
      varying float vUptake;
      uniform vec2 overlayWindow;
      uniform vec3 anatomyColor;
      uniform vec3 pigmentColor;
      void main() {
        vec3 n = normalize(vNormal);
        float diffuse = max(dot(n, normalize(vec3(-.4, .65, 1.))), 0.);
        float light = .80 + .20 * diffuse;
        float edge = pow(1. - abs(n.z), 3.5);
        float noise = fract(sin(dot(floor(vPosition * 5.), vec3(12.9898, 78.233, 34.18))) * 43758.5453);
        float hatch = smoothstep(.62, .80, sin((vPosition.x + vPosition.y + vPosition.z) * 3.));
        float shade = light * (.78 + .22 * vCavity) - .025 * edge;
        shade -= .012 * hatch * (1. - diffuse);
        shade += (noise - .5) * .025;
        // Low uptake is transparent in the pigment layer. Anatomy remains
        // opaque, so sulci and the far hemisphere do not show through it.
        float opacity = .86 * smoothstep(overlayWindow.x, overlayWindow.y, vUptake);
        vec3 color = mix(anatomyColor, pigmentColor, opacity) * shade;
        gl_FragColor = vec4(color, 1.);
      }`
  });
  scene.add(new THREE.Mesh(geometry, material));
  const outlineMaterial = new THREE.ShaderMaterial({
    side: THREE.BackSide,
    uniforms: { outlineColor: { value: new THREE.Vector3(.27, .31, .23) } },
    vertexShader: `void main() { gl_Position = projectionMatrix * modelViewMatrix * vec4(position + normal * .33, 1.); }`,
    fragmentShader: `uniform vec3 outlineColor; void main() { gl_FragColor = vec4(outlineColor, 1.); }`
  });
  scene.add(new THREE.Mesh(geometry, outlineMaterial));
  const data = new Uint16Array(uptake);
  let pair = -1, playing = false, rotating = !reducedMotion.matches;
  let frame = 0, previousTime = performance.now(), elapsed = 0;
  let contextLost = false;

  function render() { renderer.render(scene, camera); }
  function applyTheme() {
    const dark = document.documentElement.dataset.theme === 'dark';
    uniforms.anatomyColor.value.set(...(dark ? [.73, .75, .72] : [.86, .85, .78]));
    uniforms.pigmentColor.value.set(...(dark ? [.84, .31, .26] : [.72, .23, .18]));
    outlineMaterial.uniforms.outlineColor.value.set(...(dark ? [.14, .19, .16] : [.27, .31, .23]));
    render();
  }
  document.addEventListener('dccc:themechange', applyTheme);
  function updateCamera(renderNow = true) {
    const horizontal = 450 * Math.cos(elevation);
    camera.position.set(center.x + horizontal * Math.cos(azimuth), center.y + horizontal * Math.sin(azimuth), center.z + 450 * Math.sin(elevation));
    camera.lookAt(center);
    camera.updateMatrixWorld();
    if (renderNow) render();
  }
  function resize() {
    const width = stage.clientWidth, height = stage.clientHeight;
    renderer.setSize(width, height, false);
    const halfHeight = Math.max(99, 110 * height / width);
    camera.left = -halfHeight * width / height;
    camera.right = halfHeight * width / height;
    camera.top = halfHeight;
    camera.bottom = -halfHeight;
    camera.updateProjectionMatrix();
    updateCamera();
  }
  function setBurden(renderNow = true) {
    const cl = Number(slider.value);
    let index = manifest.cases.findIndex(c => c.targetCL > cl) - 1;
    if (cl >= manifest.cases.at(-1).targetCL) index = manifest.cases.length - 2;
    index = Math.max(0, index);
    const a = manifest.cases[index], b = manifest.cases[index + 1];
    const fraction = THREE.MathUtils.clamp((cl - a.targetCL) / (b.targetCL - a.targetCL), 0, 1);
    if (pair !== index) {
      for (let i = 0; i < count; i++) {
        lowAttribute.array[i] = data[index * count + i] / 10000;
        highAttribute.array[i] = data[(index + 1) * count + i] / 10000;
      }
      lowAttribute.needsUpdate = highAttribute.needsUpdate = true;
      pair = index;
    }
    // A clean 0–100 presentation scale uses the nominal selection targets.
    // Actual case CL values remain unchanged in the provenance.
    uniforms.blend.value = fraction;
    value.replaceChildren(document.createTextNode(`${Math.round(cl)} `));
    const unit = document.createElement('small'); unit.textContent = 'CL'; value.append(unit);
    slider.style.setProperty('--progress', `${100 * (cl - Number(slider.min)) / (Number(slider.max) - Number(slider.min))}%`);
    slider.setAttribute('aria-valuetext', t('sliderValue', { value: Math.round(cl) }));
    stage.dataset.lowerAnchor = a.actualCL;
    stage.dataset.upperAnchor = b.actualCL;
    stage.dataset.blend = fraction.toFixed(5);
    if (renderNow) render();
  }
  function schedule() {
    if (!frame && !contextLost && !document.hidden && (playing || rotating)) {
      frame = requestAnimationFrame(animate);
    }
  }
  function setRotation(enabled) {
    rotating = enabled;
    rotateButton.setAttribute('aria-pressed', String(rotating));
    const label = t(rotating ? 'rotationPause' : 'rotationResume');
    rotateButton.setAttribute('aria-label', label);
    rotateButton.title = label;
    if (rotating) viewButtons.forEach(button => button.setAttribute('aria-pressed', 'false'));
    previousTime = performance.now();
    schedule();
  }
  function stop() {
    playing = false;
    play.textContent = '▶';
    play.setAttribute('aria-label', t('uptakePlay'));
    play.setAttribute('aria-pressed', 'false');
  }
  function animate(time) {
    frame = 0;
    // Cap rendering at 30 fps; rotation completes a turn in about 100 seconds.
    const delta = time - previousTime;
    if (delta >= 32) {
      const dt = Math.min(delta, 100);
      previousTime = time;
      if (playing) {
        elapsed += dt;
        const min = Number(slider.min), max = Number(slider.max);
        const phase = (elapsed / 20000) % 2;
        slider.value = min + (max - min) * (phase <= 1 ? phase : 2 - phase);
        setBurden(false);
      }
      if (rotating) {
        azimuth += dt * .000063;
        updateCamera(false);
      }
      if (playing || rotating) render();
    }
    schedule();
  }
  rotateButton.addEventListener('click', () => setRotation(!rotating));
  play.addEventListener('click', () => {
    if (playing) { stop(); return; }
    playing = true;
    elapsed = (Number(slider.value) - Number(slider.min)) / (Number(slider.max) - Number(slider.min)) * 20000;
    previousTime = performance.now();
    play.textContent = 'Ⅱ';
    play.setAttribute('aria-label', t('uptakePause'));
    play.setAttribute('aria-pressed', 'true');
    schedule();
  });
  slider.addEventListener('input', () => { stop(); setBurden(); });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      stop(); cancelAnimationFrame(frame); frame = 0;
    } else {
      previousTime = performance.now(); schedule();
    }
  });
  reducedMotion.addEventListener('change', () => {
    if (reducedMotion.matches) { stop(); setRotation(false); }
  });
  viewButtons.forEach(button => button.addEventListener('click', () => {
    setRotation(false);
    if (button.dataset.view === 'left') { azimuth = 2.9; elevation = .19; }
    if (button.dataset.view === 'right') { azimuth = .24; elevation = .19; }
    if (button.dataset.view === 'top') { azimuth = Math.PI / 2; elevation = 1.55; }
    viewButtons.forEach(b => b.setAttribute('aria-pressed', String(b === button)));
    updateCamera();
  }));
  let drag = null;
  canvas.addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    setRotation(false);
    drag = [event.clientX, event.clientY];
    canvas.setPointerCapture(event.pointerId);
    canvas.classList.add('dragging');
  });
  canvas.addEventListener('pointermove', event => {
    if (!drag) return;
    azimuth -= (event.clientX - drag[0]) * .009;
    elevation = THREE.MathUtils.clamp(elevation + (event.clientY - drag[1]) * .009, -1.4, 1.55);
    drag = [event.clientX, event.clientY];
    viewButtons.forEach(b => b.setAttribute('aria-pressed', 'false'));
    updateCamera();
  });
  function releaseDrag() { drag = null; canvas.classList.remove('dragging'); }
  canvas.addEventListener('pointerup', releaseDrag);
  canvas.addEventListener('pointercancel', releaseDrag);
  canvas.addEventListener('lostpointercapture', releaseDrag);
  canvas.addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(event.key)) return;
    event.preventDefault();
    setRotation(false);
    if (event.key === 'ArrowLeft') azimuth += .12;
    if (event.key === 'ArrowRight') azimuth -= .12;
    if (event.key === 'ArrowUp') elevation += .1;
    if (event.key === 'ArrowDown') elevation -= .1;
    elevation = THREE.MathUtils.clamp(elevation, -1.4, 1.55);
    viewButtons.forEach(b => b.setAttribute('aria-pressed', 'false'));
    updateCamera();
  });
  canvas.addEventListener('webglcontextlost', event => {
    event.preventDefault(); stop(); setRotation(false);
    contextLost = true;
    cancelAnimationFrame(frame); frame = 0;
    loading.hidden = false;
    loading.classList.add('error');
    loadingLabel.textContent = t('interrupted');
    slider.disabled = play.disabled = rotateButton.disabled = true;
    viewButtons.forEach(button => { button.disabled = true; });
  });
  new ResizeObserver(resize).observe(stage);
  applyTheme(); setBurden(); resize();
  loading.hidden = true;
  stage.setAttribute('aria-busy', 'false');
  stage.dataset.ready = 'true';
  setRotation(rotating);
  viewer = { stop: () => { stop(); setRotation(false); cancelAnimationFrame(frame); frame = 0; } };
}

initialize().catch(error => {
  console.error(error);
  loading.hidden = false;
  loading.classList.add('error');
  loadingLabel.textContent = location.protocol === 'file:'
    ? t('localPreview')
    : t('loadError');
  slider.disabled = true;
  play.disabled = true;
  rotateButton.disabled = true;
  viewButtons.forEach(button => { button.disabled = true; });
  stage.setAttribute('aria-busy', 'false');
});
window.addEventListener('pagehide', () => viewer?.stop());
