"""Expande os includes do ESP-IDF em .vscode/c_cpp_properties.json.

O PlatformIO passa esses includes via -iprefix + arquivo de resposta, que o
IntelliSense do VSCode não expande (erro "cannot open source file ...").
O PlatformIO regenera o arquivo de tempos em tempos; rode de novo se o erro voltar:

    python scripts/fix_intellisense.py
"""
import json
import os
import re
import sys

PROJETO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARQUIVO = os.path.join(PROJETO, ".vscode", "c_cpp_properties.json")

with open(ARQUIVO, encoding="utf-8") as f:
    bruto = f.read()

cabecalho, corpo, no_cabecalho = [], [], True
for linha in bruto.splitlines(keepends=True):
    if no_cabecalho and linha.strip().startswith("//"):
        cabecalho.append(linha)
    else:
        no_cabecalho = False
        corpo.append(linha)

dados = json.loads("".join(corpo))
cfg = dados["configurations"][0]

args = cfg.get("compilerArgs", [])
base = next((a for i, a in enumerate(args) if i and args[i - 1] == "-iprefix"), None)
resp = next((a[1:] for a in args if a.startswith("@") and a.endswith("flags/includes")), None)
if not base or not resp or not os.path.isfile(resp):
    sys.exit("Não achei -iprefix/@flags/includes em compilerArgs (rode 'pio run' antes).")

with open(resp, encoding="utf-8") as f:
    tokens = f.read().split()

novos = [re.sub(r"/+", "/", base + tokens[i + 1]).replace(":/", ":/")
         for i, t in enumerate(tokens[:-1]) if t == "-iwithprefixbefore"]

for lista in (cfg["includePath"], cfg.get("browse", {}).get("path", [])):
    existentes = set(lista)
    lista.extend(p for p in dict.fromkeys(novos) if p not in existentes)

with open(ARQUIVO, "w", encoding="utf-8") as f:
    f.write("".join(cabecalho))
    f.write(json.dumps(dados, indent=4, ensure_ascii=False) + "\n")

print("includePath agora com", len(cfg["includePath"]), "entradas")
