import sys, io, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

with open('C:/Files/TDPS/reports/final_report/src/main.tex', 'r', encoding='utf-8') as f:
    text = f.read()

# Redundancy: repeated key claims
claims = {
    r'19\%': 0,
    r'97.65': 0,
    r'100\%': 0,
    'zero off-line time': 0,
    '63-group': 0,
    '62-parameter': 0,
    '6-parameter': 0,
    '30--40': 0,
    r'22\,cm': 0,
    'forward-only': 0,
    'curvature feedforward': 0,
    'correction standard deviation': 0,
    'regression score': 0,
    'PD+kff': 0,
    'open-loop': 0,
}

print('=== REDUNDANCY: KEY CLAIM FREQUENCY ===')
for pat, _ in claims.items():
    count = text.count(pat)
    if count >= 2:
        flag = ' *** HIGH' if count >= 5 else ''
        print(f'  "{pat}": {count}{flag}')

# Tense consistency
we_present = len(re.findall(r'\bWe\s+(use|employ|adopt|implement|apply|design|compute|propose|organize|derive|analyze)\b', text))
we_past = len(re.findall(r'\bWe\s+(used|employed|adopted|implemented|applied|designed|computed|proposed|organized|derived|analyzed|reduced|classified|determined|refactored|first)\b', text))
print(f'\n=== TENSE CONSISTENCY ("We" + verb) ===')
print(f'  Present tense: {we_present}')
print(f'  Past tense: {we_past}')

# Figure caption passives
captions = re.findall(r'\\caption\{([^}]+)\}', text)
print(f'\n=== FIGURE/TABLE CAPTIONS WITH PASSIVE VOICE ===')
for i, c in enumerate(captions):
    matches = re.findall(r'\b(is|are|was|were)\s+\w+ed\b', c)
    if matches:
        print(f'  Caption {i+1}: "{c[:100]}..."')
        print(f'    Passives: {matches}')

# Paragraph structure
# Split on double newline or section commands
paras = re.split(r'\n\s*\n', text)
long_paras = 0
for p in paras:
    # Skip LaTeX-only blocks
    if p.strip().startswith('\\') and not '\\IEEEPARstart' in p and not '\\textit' in p:
        continue
    sentences = re.split(r'[.!?]\s', p)
    real_sents = [s.strip() for s in sentences if len(s.strip()) > 15]
    if len(real_sents) > 6:
        long_paras += 1
        preview = p.strip()[:80].replace('\n', ' ')
        print(f'  Long para ({len(real_sents)} sents): "{preview}..."')
print(f'\n  Total long paragraphs (>6 sentences): {long_paras}')

# Abstract structure check
print('\n=== ABSTRACT STRUCTURE ===')
abstract = re.search(r'\\begin\{abstract\}(.*?)\\end\{abstract\}', text, re.DOTALL)
if abstract:
    abs_text = abstract.group(1).strip()
    sentences = re.split(r'(?<=[.!?])\s+', abs_text)
    for i, s in enumerate(sentences):
        wc = len(s.split())
        print(f'  S{i+1} [{wc} words]: {s[:120]}')
