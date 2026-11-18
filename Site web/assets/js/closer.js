// « De plus près » : la capture de l'instrument zoome sur chaque partie,
// avec un texte qui l'explique (repères numérotés, flèches, pastilles).

(() => {
  // Zones en pixels de la capture (x, y, largeur, hauteur).
  const INSTRUMENTS = {
    lp60: {
      image: "assets/img/lp-60.jpg", width: 2972, height: 800, ratio: 2.4,
      alt: "Interface du LP-60",
      overview: {
        fr: ["Vue d'ensemble", "Toute la façade du Juno-60, de gauche à droite : arpège, LFO, oscillateur, filtres, VCA, enveloppe et chorus. Douze sons d'usine se choisissent en haut, au centre."],
        en: ["Overview", "The whole Juno-60 front panel, left to right: arpeggiator, LFO, oscillator, filters, VCA, envelope and chorus. Twelve factory sounds are picked at the top centre."],
      },
      parts: [
        { zone: [61, 140, 366, 440],
          fr: ["Arpège", "UP, U&D ou DOWN sur 1, 2 ou 3 octaves, de 1,5 à 50 pas par seconde. HOLD garde l'accord quand on lâche les touches, SYNC cale l'arpège sur le tempo de votre logiciel."],
          en: ["Arpeggiator", "UP, U&D or DOWN over 1, 2 or 3 octaves, from 1.5 to 50 steps per second. HOLD keeps the chord when you let go, SYNC locks the arpeggio to your host's tempo."] },
        { zone: [438, 140, 283, 440],
          fr: ["LFO", "Un triangle de 0,04 à 67 Hz, assez rapide pour salir le timbre. DELAY le fait attendre puis monter doucement après chaque note, comme sur l'original."],
          en: ["LFO", "A triangle from 0.04 to 67 Hz, fast enough to grit up the tone. DELAY holds it back, then fades it in after each note, just like the original."] },
        { zone: [734, 140, 637, 440],
          fr: ["DCO", "Dent de scie, impulsion et sous-oscillateur cumulables, en 16', 8' ou 4', plus du bruit. La largeur d'impulsion suit le LFO, l'enveloppe ou la main : de 49 % à 4 %, à volume constant."],
          en: ["DCO", "Sawtooth, pulse and sub-oscillator can be stacked, at 16', 8' or 4', plus noise. Pulse width follows the LFO, the envelope or your hand: from 49 % down to 4 %, at constant level."] },
        { zone: [1376, 140, 662, 440],
          fr: ["Filtres", "Un passe-haut à quatre positions, puis le filtre 24 dB résonant à quatre étages saturés, jusqu'à l'auto-oscillation accordée sur la note. Enveloppe normale ou inversée, LFO et suivi de clavier."],
          en: ["Filters", "A four-position high-pass, then the resonant 24 dB filter with four saturating stages, up to self-oscillation tuned to the note. Normal or inverted envelope, LFO and keyboard tracking."] },
        { zone: [2045, 140, 574, 440],
          fr: ["VCA et enveloppe", "Le VCA suit l'enveloppe ou s'ouvre simplement le temps de la note (GATE). ADSR mesurée sur le JU-06A : attaque presque linéaire, decay et release exponentiels."],
          en: ["VCA and envelope", "The VCA follows the envelope or simply opens while the note is held (GATE). ADSR measured on the JU-06A: almost linear attack, exponential decay and release."] },
        { zone: [2630, 140, 283, 440],
          fr: ["Chorus", "I, II ou les deux ensemble, toujours en stéréo, avec le souffle sombre des lignes à retard BBD. NOISE règle ce souffle : à zéro, le chorus est parfaitement propre."],
          en: ["Chorus", "I, II or both together, always in stereo, with the dark hiss of the BBD delay lines. NOISE sets that hiss: at zero, the chorus is perfectly clean."] },
        { zone: [61, 575, 700, 225],
          fr: ["Jeu", "Accordage, ampleur du pitch bend vers l'oscillateur et vers le filtre, profondeur de la molette sur le LFO. Six voix en rotation ; glissez un fichier .mid sur la fenêtre pour l'écouter."],
          en: ["Playing", "Master tune, pitch-bend range to the oscillator and to the filter, mod-wheel depth on the LFO. Six rotating voices; drop a .mid file onto the window to hear it."] },
      ],
    },

    modelplay: {
      image: "assets/img/model-play.jpg", width: 2772, height: 1404, ratio: 2772 / 1404,
      alt: "Interface du Model Play",
      overview: {
        fr: ["Vue d'ensemble", "En haut, le Minimoog tel quel : contrôleurs, oscillateurs, mixer, filtre et contours. En dessous, les ajouts : LFO, effets, MIDI learn et arpégiateur. Tous neutres par défaut."],
        en: ["Overview", "At the top, the Minimoog as it was: controllers, oscillators, mixer, filter and contours. Below, the additions: LFO, effects, MIDI learn and arpeggiator. All neutral by default."],
      },
      parts: [
        { zone: [65, 91, 408, 614],
          fr: ["Contrôleurs", "Accordage, glide, mélange et profondeur de modulation, dérive analogique. POLY fait passer le synthé de mono (priorité à la note grave, legato) à 8 voix."],
          en: ["Controllers", "Tune, glide, modulation mix and amount, analog drift. POLY switches the synth from mono (low-note priority, legato) to 8 voices."] },
        { zone: [479, 91, 823, 614],
          fr: ["Oscillateurs", "Trois oscillateurs libres de LO à 2', six formes d'onde, anti-repliement et suréchantillonnage x2. L'osc 3 peut quitter le clavier et devenir une source de modulation."],
          en: ["Oscillators", "Three free-running oscillators from LO to 2', six waveforms, anti-aliasing and 2x oversampling. Osc 3 can leave the keyboard and become a modulation source."] },
        { zone: [1302, 91, 425, 614],
          fr: ["Mixer", "Les trois oscillateurs et un bruit blanc ou rose. Poussez les niveaux : le mixer sature doucement le filtre, le secret du gras de l'original."],
          en: ["Mixer", "The three oscillators plus white or pink noise. Push the levels and the mixer gently overdrives the filter, the secret of the original's fatness."] },
        { zone: [1727, 91, 829, 614],
          fr: ["Filtre et contours", "Le filtre en échelle 24 dB, saturé à chaque étage, résonance jusqu'à l'auto-oscillation. Deux contours attaque, decay, sustain : DECAY comme l'original, ou release réglable en mode ADSR."],
          en: ["Filter and contours", "The 24 dB ladder filter, saturating in every stage, resonance up to self-oscillation. Two attack, decay, sustain contours: DECAY like the original, or an adjustable release in ADSR mode."] },
        { zone: [65, 732, 807, 231],
          fr: ["LFO", "Cinq formes, vitesse libre ou synchronisée au tempo, vers les oscillateurs, l'osc 2, le filtre ou le volume. AUTOPAN le promène dans l'espace stéréo."],
          en: ["LFO", "Five shapes, free or tempo-synced rate, to all oscillators, osc 2, the filter or the volume. AUTOPAN sweeps the sound across the stereo field."] },
        { zone: [877, 732, 1679, 231],
          fr: ["Effets", "Overdrive avec coupe-bas et coupe-haut, chorus BBD en trois modes, delay stéréo synchronisable au tempo, avec une voie droite deux fois plus longue en option."],
          en: ["Effects", "Overdrive with low and high cut, three-mode BBD chorus, tempo-syncable stereo delay, with an optional right channel twice as long."] },
        { zone: [2556, 732, 216, 231],
          fr: ["MIDI learn", "Cliquez LEARN, touchez un réglage, tournez un potard de votre contrôleur : ils sont associés. RESET efface tout."],
          en: ["MIDI learn", "Click LEARN, touch a control, turn a knob on your controller: they are linked. RESET clears everything."] },
        { zone: [65, 990, 2657, 237],
          fr: ["Arpégiateur", "Cinq modes, de 1 à 4 octaves, et un motif de 16 pas qui découpe les notes en rythme."],
          en: ["Arpeggiator", "Five modes, 1 to 4 octaves, and a 16-step pattern that chops the notes into a rhythm."] },
      ],
    },
  };

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
