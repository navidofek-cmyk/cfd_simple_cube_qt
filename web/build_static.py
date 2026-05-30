"""
Vygeneruje STATICKOU verzi webu do ./site/ — k hostování na GitHub Pages.

Použije stejné šablony i obsah jako FastAPI server (server.py), jen přepne
helper `url(...)` do statického režimu (odkazy → soubory .html). Kód se opět
čte živě z disku v okamžiku buildu.

Spuštění:
    uv run python build_static.py
Výstup:
    site/index.html, site/architecture.html, site/file_<slug>.html, site/.nojekyll
"""

import shutil
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, select_autoescape

import server as S
from annotations import FILES, SOLVER_DIR

OUT = Path(__file__).parent / "site"
# Statický teoretický web řešiče (předgenerovaný v jeho docs/site)
THEORY_SRC = SOLVER_DIR / "docs" / "site"

# Vlastní Jinja prostředí (mimo Starlette) — šablony nepoužívají request.
env = Environment(
    loader=FileSystemLoader(str(S.TEMPLATES_DIR)),
    autoescape=select_autoescape(["html"]),
)

url = S.make_url(static=True)


def render(template: str, context: dict) -> str:
    return env.get_template(template).render({**context, "url": url})


def write(name: str, html: str) -> None:
    (OUT / name).write_text(html, encoding="utf-8")
    print(f"  + {name}  ({len(html)//1024} kB)")


def main() -> None:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    print("Generuji statický web do site/ …")
    write("index.html", render("index.html", S.ctx_index()))
    write("architecture.html", render("architecture.html", S.ctx_architecture()))
    for slug in FILES:
        write(f"file_{slug}.html", render("file.html", S.ctx_file(slug)))

    # .nojekyll → GitHub Pages nebude soubory přeskakovat přes Jekyll
    (OUT / ".nojekyll").write_text("", encoding="utf-8")

    # Zkopíruj teoretický web řešiče do site/theory/ (vše na jednom místě)
    if THEORY_SRC.is_dir():
        shutil.copytree(THEORY_SRC, OUT / "theory")
        n_theory = len(list((OUT / "theory").glob("*.html")))
        print(f"  + theory/  ({n_theory} stránek teoretického webu řešiče)")
    else:
        n_theory = 0
        print(f"  ! theory/ vynecháno — nenalezen {THEORY_SRC}")
        print("    (vygeneruj ho: cd ../../cfd_simple_cube/docs && uv run python build_static.py)")

    print(f"\nHotovo: {len(FILES) + 2} stránek průvodce + {n_theory} teorie v {OUT}")
    print("Náhled lokálně:  python -m http.server -d site 8090")


if __name__ == "__main__":
    main()
