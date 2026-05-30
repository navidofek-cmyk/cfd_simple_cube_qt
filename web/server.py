"""
FastAPI web — interaktivní průvodce kódem projektu CFD Cube (řešič + Qt GUI).

Web čte reálné zdrojové soubory z disku a ke každému přidá český výklad po
sekcích (viz annotations.py). Kód se zvýrazní Pygmentsem. Žádná databáze,
žádný build — `uv run` a běží.

Spuštění:
    uv run uvicorn server:app --reload
    # nebo:
    uv run python server.py
"""

from pathlib import Path

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from pygments import highlight
from pygments.lexers import CppLexer, CMakeLexer, TextLexer
from pygments.formatters import HtmlFormatter

from annotations import FILES, GROUPS

app = FastAPI(title="CFD Cube — průvodce kódem")
templates = Jinja2Templates(directory=str(Path(__file__).parent / "templates"))

# Pygments: jeden formatter (řádkování zap.), styl „monokai".
_FORMATTER = HtmlFormatter(style="monokai", nowrap=False, cssclass="hl")
_PYGMENTS_CSS = _FORMATTER.get_style_defs(".hl")

_LEXERS = {"cpp": CppLexer(), "cmake": CMakeLexer(), "text": TextLexer()}


def _lexer(lang: str):
    return _LEXERS.get(lang, _LEXERS["text"])


def _read(path: Path) -> list[str]:
    """Načte soubor jako seznam řádků (bez koncových \\n). Toleruje chybějící."""
    try:
        return path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return []


def _hl(code: str, lang: str) -> str:
    """Zvýrazní úryvek kódu na HTML."""
    return highlight(code, _lexer(lang), _FORMATTER)


def build_sections(meta: dict) -> dict:
    """
    Rozseká soubor na sekce podle 'kotev'. Kotva = unikátní úryvek; sekce sahá
    od řádku s kotvou po řádek další kotvy. Robustní vůči přečíslování řádků.
    Vrací dict s 'preamble' (před první kotvou) a 'sections' (heading/note/code/lineno).
    """
    lines = _read(Path(meta["path"]))
    if not lines:
        return {"missing": True, "preamble": "", "sections": [], "loc": 0}

    anchors = meta["sections"]
    # Najdi řádek každé kotvy (postupně, aby se neopakovaně nešahalo dozadu).
    starts: list[int | None] = []
    search_from = 0
    for a in anchors:
        idx = None
        for i in range(search_from, len(lines)):
            if a["anchor"] in lines[i]:
                idx = i
                break
        starts.append(idx)
        if idx is not None:
            search_from = idx + 1

    first = next((s for s in starts if s is not None), len(lines))
    preamble_code = "\n".join(lines[:first]).strip("\n")

    sections = []
    for n, (a, s) in enumerate(zip(anchors, starts)):
        if s is None:
            # kotva nenalezena → zobraz aspoň poznámku bez kódu
            sections.append({"heading": a["heading"], "note": a["note"],
                             "code": "", "lineno": None, "missing": True})
            continue
        # konec = další nalezená kotva, jinak konec souboru
        nxt = next((x for x in starts[n + 1:] if x is not None), len(lines))
        code = "\n".join(lines[s:nxt]).strip("\n")
        sections.append({"heading": a["heading"], "note": a["note"],
                         "code": _hl(code, meta["lang"]), "lineno": s + 1,
                         "missing": False})

    return {
        "missing": False,
        "preamble": _hl(preamble_code, meta["lang"]) if preamble_code else "",
        "sections": sections,
        "loc": len(lines),
        "full": _hl("\n".join(lines), meta["lang"]),
    }


def grouped_files() -> dict[str, list]:
    """Soubory seskupené podle GROUPS (zachová pořadí GROUPS i vložení)."""
    out: dict[str, list] = {g: [] for g in GROUPS}
    for slug, meta in FILES.items():
        out.setdefault(meta["group"], []).append((slug, meta))
    return {g: items for g, items in out.items() if items}


@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    total_loc = sum(len(_read(Path(m["path"]))) for m in FILES.values())
    # Pozn.: novější Starlette chce request jako 1. argument (ne název šablony).
    return templates.TemplateResponse(request, "index.html", {
        "groups": grouped_files(),
        "n_files": len(FILES),
        "total_loc": total_loc,
        "pygments_css": _PYGMENTS_CSS,
    })


@app.get("/architecture", response_class=HTMLResponse)
def architecture(request: Request):
    return templates.TemplateResponse(request, "architecture.html", {
        "groups": grouped_files(),
        "arch": True,
        "pygments_css": _PYGMENTS_CSS,
    })


@app.get("/file/{slug}", response_class=HTMLResponse)
def file_page(request: Request, slug: str):
    meta = FILES.get(slug)
    if meta is None:
        raise HTTPException(404, "Soubor není v průvodci.")
    data = build_sections(meta)
    # navigace předchozí/další v rámci celého pořadí
    slugs = list(FILES.keys())
    i = slugs.index(slug)
    prev = slugs[i - 1] if i > 0 else None
    nxt = slugs[i + 1] if i < len(slugs) - 1 else None
    return templates.TemplateResponse(request, "file.html", {
        "slug": slug, "meta": meta, "data": data,
        "groups": grouped_files(),
        "prev": (prev, FILES[prev]["title"]) if prev else None,
        "next": (nxt, FILES[nxt]["title"]) if nxt else None,
        "pygments_css": _PYGMENTS_CSS,
    })


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8085, reload=True)
