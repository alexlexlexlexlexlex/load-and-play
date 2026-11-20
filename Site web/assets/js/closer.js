// « De plus près » : la capture de l'instrument zoome sur chaque partie,
// avec un texte qui l'explique (repères numérotés, flèches, pastilles).

(() => {
  const INSTRUMENTS = window.LP_CLOSER || {};

  const el = (tag, cls) => {
    const e = document.createElement (tag);
    if (cls) e.className = cls;
    return e;
  };

  // Texte bilingue : deux balises, la feuille de style montre la bonne.
  const both = (tag, pair, cls) => {
    const frag = document.createDocumentFragment ();
    for (const lang of ["fr", "en"]) {
      const e = el (tag, cls);
      e.lang = lang;
      e.textContent = pair[lang];
      frag.append (e);
    }
    return frag;
  };

  function build (section, data) {
    const stage = el ("div", "closer-stage");
    stage.style.setProperty ("--ratio", data.ratio);
    const img = el ("img");
    img.src = data.image;
    img.alt = data.alt;
    img.width = data.width;
    img.height = data.height;
    img.draggable = false;
    stage.append (img);

    const spots = data.parts.map ((part, i) => {
      const b = el ("button", "closer-spot");
      b.type = "button";
      b.textContent = i + 1;
      b.setAttribute ("aria-label", part.fr[0] + " / " + part.en[0]);
      b.addEventListener ("click", e => { e.stopPropagation (); show (i + 1); });
      stage.append (b);
      return b;
    });

    const bar = el ("div", "closer-bar");
    const prev = el ("button", "closer-arrow");
    const next = el ("button", "closer-arrow");
    prev.type = next.type = "button";
    prev.textContent = "‹";
    next.textContent = "›";
    prev.setAttribute ("aria-label", "Précédent / Previous");
    next.setAttribute ("aria-label", "Suivant / Next");
    const card = el ("div", "closer-card");
    card.setAttribute ("aria-live", "polite");
    bar.append (prev, card, next);

    const dots = el ("div", "closer-dots");
    const pages = [{ fr: data.overview.fr, en: data.overview.en }, ...data.parts];
    const dotButtons = pages.map ((page, i) => {
      const b = el ("button");
      b.type = "button";
      b.append (i === 0 ? both ("span", { fr: "Vue d'ensemble", en: "Overview" }) : document.createTextNode (`${i}. `));
      if (i > 0) b.append (both ("span", { fr: page.fr[0], en: page.en[0] }));
      b.addEventListener ("click", () => show (i));
      dots.append (b);
      return b;
    });

    section.append (stage, bar, dots);

    let current = 0;
    const count = pages.length;

    function layout () {
      const sw = stage.clientWidth, sh = stage.clientHeight;
      const zone = current === 0 ? [0, 0, data.width, data.height] : pages[current].zone;
      const [x, y, w, h] = zone;
      const pad = current === 0 ? 1 : 1.1;
      const s = Math.min (sw / (w * pad), sh / (h * pad));
      const place = (size, view, centre) => {
        const full = size * s;
        if (full <= view) return (view - full) / 2;
        return Math.min (0, Math.max (view - full, view / 2 - centre * s));
      };
      const tx = place (data.width, sw, x + w / 2);
      const ty = place (data.height, sh, y + h / 2);
      img.style.transform = `translate(${tx}px, ${ty}px) scale(${s})`;
      // Les repères suivent la vue d'ensemble.
      data.parts.forEach ((part, i) => {
        const [px, py, pw, ph] = part.zone;
        spots[i].style.left = `${tx + (px + pw / 2) * s}px`;
        spots[i].style.top = `${ty + (py + ph / 2) * s}px`;
      });
    }

    function show (i) {
      current = (i + count) % count;
      section.classList.toggle ("zoomed", current > 0);
      const page = pages[current];
      card.innerHTML = "";
      const num = el ("div", "num");
      num.append (current === 0 ? both ("span", { fr: "Présentation", en: "Introduction" })
                                : document.createTextNode (`${String (current).padStart (2, "0")} / ${String (count - 1).padStart (2, "0")}`));
      card.append (num, both ("h3", { fr: page.fr[0], en: page.en[0] }), both ("p", { fr: page.fr[1], en: page.en[1] }));
      dotButtons.forEach ((b, k) => b.setAttribute ("aria-current", String (k === current)));
      layout ();
    }

    prev.addEventListener ("click", () => show (current - 1));
    next.addEventListener ("click", () => show (current + 1));
    stage.addEventListener ("click", () => { if (current > 0) show (0); });
    section.addEventListener ("keydown", e => {
      if (e.target.closest ("input, select, textarea")) return;
      if (e.key === "ArrowLeft") { show (current - 1); e.preventDefault (); }
      if (e.key === "ArrowRight") { show (current + 1); e.preventDefault (); }
    });
    // Première mise en place sans animation (sinon l'image part de sa taille réelle).
    img.style.transition = "none";
    show (0);
    new ResizeObserver (() => {
      layout ();
      requestAnimationFrame (() => (img.style.transition = ""));
    }).observe (stage);
  }

  document.querySelectorAll ("[data-closer]").forEach (section => {
    const data = INSTRUMENTS[section.dataset.closer];
    if (data) build (section, data);
  });
}) ();
