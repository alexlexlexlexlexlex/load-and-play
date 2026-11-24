// Comportements communs à toutes les pages : langue, contact, liens de
// téléchargement et blocs « click to try ».
(function () {
  const site = window.SITE || {};
  const root = document.documentElement;

  //--------------------------------------------------------------------------
  // Langue. Chaque page existe en deux adresses : « lp-60.html » (français) et
  // « lp-60-en.html » (anglais, fabriquée à la mise en ligne par
  // tools/seo/make_en.py). La langue affichée est celle choisie avec le bouton
  // FR / EN, sinon celle de l'adresse : jamais celle du navigateur, pour que
  // Google voie chaque page dans sa langue. Le bouton bascule sur place,
  // sans recharger, et met l'adresse de l'autre langue dans la barre.
  function storedLanguage () {
    try { return localStorage.getItem ("lp-lang"); } catch (e) { return null; }
  }

  const PAGE = /^([a-z0-9-]+?)(-en)?\.html(#.*)?$/;

  // Adresse d'une page du site dans la langue voulue (null si ce n'en est pas une).
  function pageIn (lang, href) {
    const m = PAGE.exec (href);
    if (! m) return null;
    if (lang === "en") return `${m[1]}-en.html${m[3] || ""}`;
    return (m[1] === "index" && ! m[3] ? "./" : `${m[1]}.html`) + (m[3] || "");
  }

  // Textes qui changent avec la langue hors des balises lang="…" :
  // titre de l'onglet et attributs alt / aria-label (data-fr-… et data-en-…).
  function swapTexts (lang) {
    const t = document.querySelector ("title[data-en-text]");
    if (t) {
      if (! t.dataset.frText) t.dataset.frText = t.textContent;
      document.title = lang === "en" ? t.dataset.enText : t.dataset.frText;
    }
    for (const attr of ["alt", "aria-label"]) {
      const key = attr.replace (/-(\w)/g, (m, c) => c.toUpperCase ());
      const Key = key[0].toUpperCase () + key.slice (1);
      document.querySelectorAll (`body [data-en-${attr}]`).forEach (el => {
        if (! el.dataset["fr" + Key]) el.dataset["fr" + Key] = el.getAttribute (attr);
        el.setAttribute (attr, el.dataset[lang + Key]);
      });
    }
  }

  function setLanguage (lang) {
    root.dataset.uiLang = lang;
    root.lang = lang;
    document.querySelectorAll (".lang-switch button").forEach (b =>
      b.setAttribute ("aria-pressed", String (b.dataset.lang === lang)));
    swapTexts (lang);

    // Liens internes vers les pages de la même langue.
    document.querySelectorAll ("a[href]").forEach (a => {
      const target = pageIn (lang, a.getAttribute ("href"));
      if (target) a.setAttribute ("href", target === "./" ? "index.html" : target);
    });

    // Adresse de la page dans la barre (pas en file://).
    if (/^https?:$/.test (location.protocol)) {
      const file = location.pathname.split ("/").pop ();      // "" = accueil
      const target = pageIn (lang, file || "index.html");
      const same = target === "./" ? (file === "" || file === "index.html") : target === file;
      if (target && ! same)
        history.replaceState (history.state, "", target + location.search + location.hash);
    }
    document.dispatchEvent (new CustomEvent ("languagechange", { detail: lang }));
  }

  const pageLanguage = root.lang === "en" ? "en" : "fr";
  const initial = storedLanguage () || pageLanguage;
  // Appliqué tout de suite (ce script est dans <head>) : pas d'éclair de l'autre langue.
  root.dataset.uiLang = initial;
  root.lang = initial;

  document.addEventListener ("DOMContentLoaded", () => {
    setLanguage (initial);
    document.querySelectorAll (".lang-switch button").forEach (b =>
      b.addEventListener ("click", () => {
        try { localStorage.setItem ("lp-lang", b.dataset.lang); } catch (e) { /* navigation privée */ }
        setLanguage (b.dataset.lang);
      }));

    //------------------------------------------------------------------------
    // Contact
    document.querySelectorAll ("[data-mail]").forEach (a => {
      if (site.email) a.href = "mailto:" + site.email;
      else a.hidden = true;
    });
    document.querySelectorAll ("[data-mail-text]").forEach (el => {
      el.textContent = site.email || "…";
    });

    //------------------------------------------------------------------------
    // Téléchargements et code : liens vers GitHub, ou « bientôt » tant que
    // le dépôt n'est pas renseigné.
    const gh = site.github || {};
    const base = gh.user ? `https://github.com/${gh.user}/${gh.repo}` : "";
    const versions = site.versions || {};

    document.querySelectorAll ("[data-asset]").forEach (a => {
      const name = a.dataset.asset.replace (/\{(\w+)\}/g, (m, key) => versions[key] || "");
      if (base) {
        a.href = `${base}/releases/latest/download/${encodeURIComponent (name)}`;
      } else {
        a.removeAttribute ("href");
        a.classList.add ("disabled");
        a.title = root.dataset.uiLang === "en" ? "Available soon" : "Bientôt disponible";
      }
    });

    document.querySelectorAll ("[data-github]").forEach (a => {
      if (gh.user) a.href = `https://github.com/${gh.user}`;
      else a.hidden = true;
    });

    document.querySelectorAll ("[data-repo]").forEach (a => {
      if (base) a.href = `${base}/tree/main/${a.dataset.repo}`;
      else { a.removeAttribute ("href"); a.classList.add ("disabled"); }
    });

    //------------------------------------------------------------------------
    // « Click to try » : ouvre le bloc blanc ; l'instrument ne se charge
    // qu'au premier clic (le son ne peut démarrer qu'après un geste).
    // Un seul bloc ouvert à la fois : en ouvrir un referme les autres.
    const toggles = [...document.querySelectorAll ("[data-try]")]
      .map (button => ({ button, panel: document.getElementById (button.dataset.try) }))
      .filter (t => t.panel);
    const setOpen = (t, open) => {
      t.panel.hidden = ! open;
      t.button.setAttribute ("aria-expanded", String (open));
      t.panel.dispatchEvent (new CustomEvent (open ? "tryopen" : "tryclose"));
    };
    toggles.forEach (t => t.button.addEventListener ("click", () => {
      if (! t.panel.hidden) { setOpen (t, false); return; }
      toggles.filter (o => o !== t && ! o.panel.hidden).forEach (o => setOpen (o, false));
      setOpen (t, true);
      t.button.scrollIntoView ({ behavior: "smooth", block: "start" });
    }));
  });
}) ();
