#!/usr/bin/env python3
"""Fabrique la version anglaise de chaque page du site Load & Play.

Les pages sources (« lp-60.html »…) sont en français et contiennent déjà le
texte anglais (balises lang="en"). Pour chaque page, ce script écrit à côté
« lp-60-en.html » : même contenu, mais servi en anglais, pour que Google
indexe chaque langue à sa propre adresse (balises hreflang).

Ce qui change dans la copie anglaise :
- <html lang="en" data-ui-lang="en"> ;
- les attributs data-en-… : <title data-en-text>, content="…" data-en-content,
  alt="…" data-en-alt, aria-label="…" data-en-aria-label (le français est gardé
  dans data-fr-…, pour le bouton FR / EN) ;
- canonical, og:url et og:locale ;
- les liens vers les autres pages (« lp-60.html » → « lp-60-en.html ») ;
- les données structurées JSON-LD (adresses, description, langue).

Il vérifie aussi que sitemap.xml cite toutes les pages.

Usage : python3 tools/seo/make_en.py <dossier du site>
  - à la mise en ligne : sur _site (voir .github/workflows/site.yml) ;
  - pour un aperçu local : sur une copie du dossier « Site web ».
Ne pas le lancer sur « Site web » lui-même : les pages -en.html ne sont pas
gardées dans le dépôt.
"""
import html
import json
import re
import sys
from pathlib import Path

BASE = "https://loadandplaymusic.com/"
PAGE_LINK = re.compile (r'(href=")([a-z0-9-]+?)\.html(#[^"]*)?"')


def fr_url (page): return BASE if page == "index.html" else BASE + page
def en_url (page): return BASE + page.replace (".html", "-en.html")


def swap_attributes (doc):
    """X="fr" … data-en-X="en"  →  X="en" data-fr-X="fr" … data-en-X="en", balise par balise."""
    def tag (m):
        t = m.group (0)
        for attr in ("content", "alt", "aria-label"):
            en = re.search (r'\sdata-en-%s="([^"]*)"' % attr, t)
            fr = re.search (r'\s%s="([^"]*)"' % attr, t)
            if en and fr:
                t = t.replace (fr.group (0), ' %s="%s" data-fr-%s="%s"' % (attr, en.group (1), attr, fr.group (1)), 1)
        return t
    doc = re.sub (r'<(meta|img|a|button|div|span)\s[^>]*data-en-[^>]*>', tag, doc)

    def title (m):
        return '<title data-fr-text="%s" data-en-text="%s">%s</title>' % (
            html.escape (html.unescape (m.group (2)), quote=True), m.group (1), m.group (1))
    return re.sub (r'<title data-en-text="([^"]*)">(.*?)</title>', title, doc, count=1, flags=re.S)


def meta_content (doc, name):
    m = re.search (r'<meta name="%s" content="([^"]*)"' % name, doc)
    return html.unescape (m.group (1)) if m else None


def make_en (src, pages):
    page = src.name
    doc = src.read_text (encoding="utf-8")
    fr_desc = meta_content (doc, "description")

    doc = doc.replace ('<html lang="fr" data-ui-lang="fr">', '<html lang="en" data-ui-lang="en">', 1)
    doc = swap_attributes (doc)
    en_desc = meta_content (doc, "description")

    doc = doc.replace ('<link rel="canonical" href="%s">' % fr_url (page),
                       '<link rel="canonical" href="%s">' % en_url (page), 1)
    doc = doc.replace ('<meta property="og:url" content="%s">' % fr_url (page),
                       '<meta property="og:url" content="%s">' % en_url (page), 1)
    doc = doc.replace ('<meta property="og:locale" content="fr_FR">', '<meta property="og:locale" content="en_US">', 1)
    doc = doc.replace ('<meta property="og:locale:alternate" content="en_US">',
                       '<meta property="og:locale:alternate" content="fr_FR">', 1)

    # Liens internes (relatifs, sans dossier) vers les pages anglaises.
    def link (m):
        name = m.group (2)
        if name + ".html" not in pages or name.endswith ("-en"):
            return m.group (0)
        return '%s%s-en.html%s"' % (m.group (1), name, m.group (3) or "")
    doc = PAGE_LINK.sub (link, doc)

    # Données structurées.
    fr_to_en = {fr_url (p): en_url (p) for p in pages}
    def walk (v, key=None):
        if isinstance (v, dict):  return {k: walk (x, k) for k, x in v.items ()}
        if isinstance (v, list):  return [walk (x) for x in v]
        if isinstance (v, str):
            if key in ("url", "@id") and v in fr_to_en: return fr_to_en[v]
            if key == "description" and v == fr_desc and en_desc: return en_desc
            if key == "inLanguage" and v == "fr": return "en"
        return v
    def ld (m):
        data = walk (json.loads (m.group (1)))
        body = json.dumps (data, ensure_ascii=False, indent=2).replace ("\n", "\n  ")
        return '<script type="application/ld+json">\n  %s\n  </script>' % body
    doc = re.sub (r'<script type="application/ld\+json">\s*(.*?)\s*</script>', ld, doc, flags=re.S)

    out = src.with_name (page.replace (".html", "-en.html"))
    out.write_text (doc, encoding="utf-8")
    return out


def check_sitemap (site, pages):
    sm = site / "sitemap.xml"
    if not sm.exists ():
        print ("ATTENTION : pas de sitemap.xml")
        return
    text = sm.read_text (encoding="utf-8")
    for p in pages:
        for url in (fr_url (p), en_url (p)):
            if "<loc>%s</loc>" % url not in text:
                print ("ATTENTION : %s absent de sitemap.xml" % url)


def main ():
    site = Path (sys.argv[1])
    # Seulement les vraies pages (pas le fichier de validation de Google, par exemple).
    pages = sorted (p.name for p in site.glob ("*.html")
                    if not p.name.endswith ("-en.html")
                    and '<html lang="fr"' in p.read_text (encoding="utf-8", errors="ignore"))
    for p in pages:
        print ("→", make_en (site / p, pages).name)
    check_sitemap (site, pages)


if __name__ == "__main__":
    main ()
