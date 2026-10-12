#!/usr/bin/env python3
"""Gera os arquivos que acompanham uma release, a partir das tags do git.

    release_files.py --tag v1.2.3 --repo Dono/Repo --artifacts pasta --out pasta

Escreve:
  CHANGELOG.md        histórico completo, uma seção por tag (na raiz do repo)
  <out>/notes.md      só a seção da tag pedida; vira o texto da release
  <out>/latest.json   o que o app consulta para avisar que há versão nova
"""

import argparse
import json
import pathlib
import re
import subprocess

# Commits que não interessam a quem usa o app.
SKIP = re.compile(r"^(CI:|Atualiza o CHANGELOG)")


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout.strip()


def sections():
    """[(tag, data, [linhas])], da versão mais nova para a mais antiga."""
    tags = [tag for tag in git("tag", "--list", "v*", "--sort=-v:refname").splitlines() if tag]
    result = []
    for index, tag in enumerate(tags):
        previous = tags[index + 1] if index + 1 < len(tags) else None
        revisions = f"{previous}..{tag}" if previous else tag
        subjects = git("log", "--no-merges", "--format=%s", revisions).splitlines()
        lines = [f"- {subject}" for subject in subjects if subject and not SKIP.match(subject)]
        result.append((tag, git("log", "-1", "--format=%cs", tag), lines or ["- Ajustes internos."]))
    return result


def download_kind(name):
    if name.endswith("-setup.exe"):
        return "windows"
    if name.endswith("-portable.zip"):
        return "windows-portable"
    if name.endswith(".deb"):
        return "deb"
    if name.endswith(".pkg.tar.zst"):
        return "arch"
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--artifacts", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    history = sections()
    current = next((section for section in history if section[0] == args.tag), None)
    if current is None:
        raise SystemExit(f"A tag {args.tag} não existe neste repositório.")

    changelog = ["# Histórico de versões", ""]
    for tag, date, lines in history:
        changelog += [f"## {tag} ({date})", "", *lines, ""]
    pathlib.Path("CHANGELOG.md").write_text("\n".join(changelog), encoding="utf-8")

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    notes = "\n".join(current[2])
    (out / "notes.md").write_text(notes + "\n", encoding="utf-8")

    base = f"https://github.com/{args.repo}/releases"
    downloads = {}
    for path in sorted(pathlib.Path(args.artifacts).iterdir()):
        kind = download_kind(path.name)
        if kind:
            downloads[kind] = f"{base}/download/{args.tag}/{path.name}"

    latest = {
        "version": args.tag.removeprefix("v"),
        "date": current[1],
        "page": f"{base}/tag/{args.tag}",
        "notes": notes,
        "downloads": downloads,
    }
    (out / "latest.json").write_text(json.dumps(latest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
