// Comportements communs à toutes les pages : langue, contact, liens de
// téléchargement et blocs « click to try ».
(function () {
  const site = window.SITE || {};
  const root = document.documentElement;

  //--------------------------------------------------------------------------
  // Langue : choix mémorisé, sinon celle du navigateur.
  function storedLanguage () {
    try { return localStorage.getItem ("lp-lang"); } catch (e) { return null; }
  }

  function setLanguage (lang) {
    root.dataset.uiLang = lang;
    root.lang = lang;
    try { localStorage.setItem ("lp-lang", lang); } catch (e) { /* navigation privée */ }
    document.querySelectorAll (".lang-switch button").forEach (b =>
      b.setAttribute ("aria-pressed", String (b.dataset.lang === lang)));
    document.dispatchEvent (new CustomEvent ("languagechange", { detail: lang }));
  }

  const initial = storedLanguage () || ((navigator.language || "fr").toLowerCase ().startsWith ("fr") ? "fr" : "en");

  document.addEventListener ("DOMContentLoaded", () => {
    setLanguage (initial);
    document.querySelectorAll (".lang-switch button").forEach (b =>
      b.addEventListener ("click", () => setLanguage (b.dataset.lang)));

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
      const name = a.dataset.asset
        .replace ("{lp60}", versions.lp60 || "")
        .replace ("{modelplay}", versions.modelplay || "");
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
