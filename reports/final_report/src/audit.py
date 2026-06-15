import sys, io, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

with open('C:/Files/TDPS/reports/final_report/src/main.tex', 'r', encoding='utf-8') as f:
    text = f.read()

# Strip comments and extract body
lines = text.split('\n')
body_lines = []
in_document = False
for line in lines:
    stripped = line.strip()
    if stripped.startswith('%'):
        continue
    if r'\begin{document}' in line:
        in_document = True
        continue
    if r'\end{document}' in line:
        break
    if in_document:
        body_lines.append(stripped)

body = ' '.join(body_lines)

# Remove LaTeX commands
clean = re.sub(r'\\cite\{[^}]*\}', '', body)
clean = re.sub(r'\\ref\{[^}]*\}', '', clean)
clean = re.sub(r'\\label\{[^}]*\}', '', clean)
clean = re.sub(r'\\[a-zA-Z]+\{', ' ', clean)
clean = re.sub(r'\\[a-zA-Z]+', ' ', clean)
clean = re.sub(r'[{}$\\~]', ' ', clean)
clean = clean.replace('--', ' ')
clean = re.sub(r'[^a-zA-Z0-9 .,;:\'-]', ' ', clean)
clean = re.sub(r'\s+', ' ', clean).strip()

# Split into sentences (period + space + uppercase)
sentences = re.split(r'(?<=[.!?])\s+(?=[A-Z])', clean)

print('=== SENTENCES > 40 WORDS ===')
count_long = 0
for s in sentences:
    wc = len(s.split())
    if wc > 40:
        count_long += 1
        print(f'  [{wc} words] {s[:200]}')
print(f'Total: {count_long}')

# Fillers in raw LaTeX (more reliable)
print('\n=== FILLER PHRASES ===')
fillers = [
    'in order to', 'it is important to note', 'the fact that',
    'due to the fact that', 'this is critical', 'it should be noted',
    'it is worth noting', 'in the event that', 'a large number of',
    'at this point in time', 'in light of the fact', 'with regard to',
    'in terms of', 'on a regular basis', 'for the purpose of'
]
for f in fillers:
    c = text.lower().count(f)
    if c > 0:
        # find context
        idx = text.lower().find(f)
        start = max(0, idx-50)
        end = min(len(text), idx+len(f)+50)
        print(f'  "{f}": {c}')
        print(f'    ...{text[start:end].strip()}...')

# Hedging
print('\n=== HEDGING ===')
hedges = [
    'it appears that', 'may potentially', 'might potentially', 'it seems',
    'could potentially', 'appears to be', 'seems to suggest',
    'it is possible that', 'one could argue', 'it could be argued',
    'remains to be seen', 'somewhat', 'rather', 'perhaps'
]
for h in hedges:
    c = text.lower().count(h)
    if c > 0:
        print(f'  "{h}": {c}')

# Passive voice detection (rough heuristic: is/are/was/were/been + past participle)
print('\n=== PASSIVE VOICE CANDIDATES (rough heuristic) ===')
passive_patterns = [
    r'\bis\s+\w+ed\b', r'\bare\s+\w+ed\b', r'\bwas\s+\w+ed\b',
    r'\bwere\s+\w+ed\b', r'\bbeen\s+\w+ed\b', r'\bis\s+\w+en\b',
    r'\bare\s+\w+en\b', r'\bwas\s+\w+en\b', r'\bwere\s+\w+en\b'
]
passive_count = 0
for line_num, line in enumerate(body_lines, 1):
    for pat in passive_patterns:
        matches = re.findall(pat, line, re.IGNORECASE)
        if matches:
            # Filter false positives
            for m in matches:
                word = m.split()[-1]
                if word not in ['used', 'based', 'required', 'designed', 'configured', 'defined', 'enabled', 'determined', 'composed', 'limited', 'initialized', 'suppressed', 'classified', 'measured', 'verified', 'triggered', 'characterized', 'separated', 'computed', 'compared', 'validated', 'derived', 'derived', 'reserved', 'combined', 'advanced', 'established', 'connected', 'specified', 'allowed', 'delivered', 'indicated', 'provided', 'presented', 'organized', 'included', 'introduced', 'produced', 'observed', 'confirmed', 'satisfied', 'addressed', 'explored', 'relaxed', 'sacrificed', 'hindering', 'achieved', 'discussed', 'noted', 'mentioned', 'identified']:
                    pass  # include real passives
            passive_count += len(matches)
            context = line.strip()[:120]
            print(f'  Line ~{line_num}: {matches}')
            print(f'    {context}')

print(f'\nRough passive count: {passive_count}')

# Concision
print('\n=== CONCISION OPPORTUNITIES ===')
verbose = {
    'in order to': 'to',
    'due to the fact that': 'because',
    'at this point in time': 'now',
    'in the event that': 'if',
    'a large number of': 'many',
    'in light of': 'given',
    'with regard to': 'regarding',
    'on account of': 'because of',
    'for the purpose of': 'for',
    'in the near future': 'soon',
    'in a timely manner': 'promptly',
    'it is important to note that': '[delete]',
    'has the ability to': 'can',
    'is able to': 'can'
}
for v, repl in verbose.items():
    c = text.lower().count(v)
    if c > 0:
        print(f'  "{v}" -> "{repl}": {c}')
