// split-langs.js — transforms the mixed bilingual pages/ site into two
// fully separate language trees: pages/en/ (English only) and
// pages/zh/ (Chinese only), with pages/index.html as the chooser.
// Run: node pages/_static/split-langs.js
const fs = require('fs');
const path = require('path');
const base = 'D:/Tech/Projects/Autohotkey/Lib/visual_studio/tasks/2026-06-07-pillow-c-foundation/pages';

function read(p) { return fs.readFileSync(p, 'utf8'); }
function write(p, s) { fs.mkdirSync(path.dirname(p), { recursive: true }); fs.writeFileSync(p, s, 'utf8'); }
function cjk(s) { return /[\u4e00-\u9fff]/.test(s); }

const NAV = {
  'Overview / 概述': ['Overview', '概述'],
  'Numpy / NumPy 互操作': ['Numpy', 'NumPy 互操作'],
  'Constants / 常量': ['Constants', '常量'],
  'Pillow (entry) / 入口': ['Pillow (entry)', 'Pillow(入口)'],
  'Boundaries / 行为边界': ['Boundaries', '行为边界'],
};

function splitSpanBlocks(html, keep) {
  // remove <span class="X">…</span> for the dropped language; keep for kept
  const drop = keep === 'en' ? 'zh' : 'en';
  return html
    .replace(new RegExp('<span class="' + drop + '">[\\s\\S]*?<\\/span>', 'g'), '')
    .replace(new RegExp('<p class="' + drop + '">[\\s\\S]*?<\\/p>', 'g'), '');
}

function splitTables(html, keep) {
  // inside tables: drop the other language's spans, unwrap kept spans to <span>.
  // NOTE: tables with plain (span-less) language COLUMNS — e.g.
  //   <th>Property</th><th>EN</th><th>中文</th> — need per-column mapping on top
  // of this step: en keeps columns 1..2, zh keeps columns 1 and 3. The 2026-08
  // one-shot repair (repair-pages-tables.js) did exactly that; keep the rule in
  // sync if those tables are ever edited.
  const drop = keep === 'en' ? 'zh' : 'en';
  return html.replace(/<table[\s\S]*?<\/table>/g, (t) =>
    t
      .replace(new RegExp('<span class="' + drop + '">[\\s\\S]*?<\\/span>', 'g'), '')
      .replace(/<span class="(en|zh)">/g, '<span>'));
}

function navLabel(html, keep) {
  let out = html;
  for (const [mixed, pair] of Object.entries(NAV)) {
    const label = keep === 'en' ? pair[0] : pair[1];
    out = out.split('>' + mixed + '<').join('>' + label + '<');
  }
  return out;
}

function headSplit(text, keep) {
  // "中文 · English" headings -> pick side; otherwise unchanged
  if (text.includes(' · ')) {
    const parts = text.split(' · ');
    return keep === 'en' ? parts[parts.length - 1] : parts[0];
  }
  return text;
}

function splitComments(code, keep) {
  const lines = code.split('\n');
  const out = [];
  for (let line of lines) {
    const m = line.match(/^(\s*;)(.*)$/);
    if (!m) { out.push(line); continue; }
    let comment = m[2];
    const idx = comment.indexOf(' / ');
    if (idx >= 0) {
      comment = keep === 'en' ? comment.slice(idx + 3).trim() : comment.slice(0, idx).trim();
    } else if (cjk(comment) && keep === 'en') {
      out.push(''); // pure-Chinese comment: drop from the EN tree
      continue;
    }
    out.push(m[1] + (comment ? ' ' + comment : ''));
  }
  return out.join('\n');
}

function build(html, keep, srcRel, outRel) {
  const other = keep === 'en' ? 'zh' : 'en';
  const otherRel = other + '/' + srcRel;

  let out = html;
  // language markers: strip the other language's spans/paragraphs
  out = splitSpanBlocks(out, keep);
  // tables: strip the other language's spans inside cells
  out = splitTables(out, keep);
  // heading splits
  out = out.replace(/<h2[^>]*>([\s\S]*?)<\/h2>/g, (m, inner) => {
    const kept = headSplit(inner.trim(), keep);
    return m.replace(inner, kept);
  });
  out = out.replace(/<h1[^>]*>([\s\S]*?)<\/h1>/g, (m, inner) => {
    let kept = headSplit(inner.trim(), keep);
    if (keep === 'zh') kept = kept.replace('>module<', '>模块<');
    return m.replace(inner, kept);
  });
  // nav labels
  out = navLabel(out, keep);
  // side-sub + title + crumbs
  out = out.replace('双语文档 · Bilingual Docs', keep === 'en' ? 'Bilingual Docs' : '双语文档');
  out = out.replace(/<title>([\s\S]*?)<\/title>/, (m, t) => {
    const name = t.split(' — pillow-c ')[0];
    return '<title>' + name + (keep === 'en' ? ' — pillow-c documentation' : ' — pillow-c 文档') + '</title>';
  });
  out = out.replace('>Home »<', '>' + (keep === 'en' ? 'Home' : '首页') + ' »<');
  // th labels for combined bilingual columns
  out = out.replace(/<th>EN \/ 中文<\/th>/g, keep === 'en' ? '<th>EN</th>' : '<th>中文</th>');
  out = out.replace(/<th>Module<\/th><th>EN<\/th><th>中文<\/th>/g, keep === 'en' ? '<th>Module</th><th>EN</th>' : '<th>模块</th><th>中文</th>');
  // data-lang attribute
  out = out.replace(' data-lang="both"', ' data-lang="' + keep + '"');
  // example code comments
  out = out.replace(/<pre class="example">([\s\S]*?)<\/pre>/g, (m, code) => {
    return m.replace(code, splitComments(code, keep));
  });
  // lang-switch: one cross-link to the other language's counterpart
  out = out.replace(/<div class="lang-switch">[\s\S]*?<\/div>/, (m) => {
    const label = keep === 'en' ? '中文' : 'English';
    return '<div class="lang-switch"><a class="lang-alt" href="' + otherRel + '">' + label + '</a></div>';
  });
  // paths: same-tree navigation + shared _static
  out = out.split('../index.html').join('index.html');
  const depth = outRel.split('/').length - 1; // 0 for index, 1 for reference
  const staticPrefix = depth === 0 ? '_static/' : '../_static/';
  out = out.split('../_static/').join(staticPrefix);
  // external language-switch styling stays in the css; pages.js is kept but inert
  return out;
}

const refDir = path.join(base, 'reference');
const refPages = fs.readdirSync(refDir).filter(f => f.endsWith('.html'));

for (const f of refPages) {
  const html = read(path.join(refDir, f));
  const en = build(html, 'en', 'reference/' + f, 'en/reference/' + f);
  const zh = build(html, 'zh', 'reference/' + f, 'zh/reference/' + f);
  write(path.join(base, 'en', 'reference', f), en);
  write(path.join(base, 'zh', 'reference', f), zh);
}

// index.html: build EN/ZH versions from the mixed index
const indexHtml = read(path.join(base, 'index.html'));
const enIndex = build(indexHtml, 'en', 'index.html', 'en/index.html')
  .replace(/<tr><td><a href="reference\/([^"]+)">([^<]+)<\/a><\/td><td>([\s\S]*?)<\/td><td>[\s\S]*?<\/td><\/tr>/g, (m, href, name, enCell) =>
    '<tr><td><a href="reference/' + href + '">' + name + '</a></td><td>' + enCell + '</td></tr>')
  .replace(/<tr><td><a href="reference\/([^"]+)">([^<]+)<\/a><\/td><td>(<a href="https:[^"]*">cnumpy<\/a>[^<]*)<\/td><td>[\s\S]*?<\/td><\/tr>/g, (m, href, name, enCell) =>
    '<tr><td><a href="reference/' + href + '">' + name + '</a></td><td>' + enCell + '</td></tr>');
write(path.join(base, 'en', 'index.html'), enIndex);

const zhIndex = build(indexHtml, 'zh', 'index.html', 'zh/index.html')
  .replace(/<tr><td><a href="reference\/([^"]+)">([^<]+)<\/a><\/td><td>[\s\S]*?<\/td><td>([\s\S]*?)<\/td><\/tr>/g, (m, href, name, zhCell) =>
    '<tr><td><a href="reference/' + href + '">' + name + '</a></td><td>' + zhCell + '</td></tr>')
  .replace(/<tr><td><a href="reference\/([^"]+)">([^<]+)<\/a><\/td><td>[\s\S]*?<\/td><td>(<a href="https:[^"]*">cnumpy<\/a>[^<]*)<\/td><\/tr>/g, (m, href, name, zhCell) =>
    '<tr><td><a href="reference/' + href + '">' + name + '</a></td><td>' + zhCell + '</td></tr>');
write(path.join(base, 'zh', 'index.html'), zhIndex);

// chooser at pages/index.html
const chooser = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>pillow-c — 文档 · Documentation</title>
<link rel="stylesheet" href="_static/pages.css">
<style>
.choose { display: flex; gap: 28px; align-items: center; justify-content: center; min-height: 80vh; font-family: "Segoe UI", "Microsoft YaHei", sans-serif; }
.choose a { text-decoration: none; border: 2px solid #2980b9; border-radius: 10px; padding: 34px 56px; font-size: 26px; color: #2980b9; background: #f3f6f6; }
.choose a:hover { background: #2980b9; color: #fff; }
.choose small { display: block; font-size: 13px; margin-top: 8px; color: #777; }
h1 { text-align: center; font-family: "Segoe UI", "Microsoft YaHei", sans-serif; color: #222; padding-top: 8vh; margin: 0 0 -14vh 0; }
</style>
</head>
<body>
<h1>pillow-c 文档 · Documentation</h1>
<div class="choose">
  <a href="en/index.html">English<small>Bilingual reference site — English version</small></a>
  <a href="zh/index.html">中文<small>双语参考站点 — 中文版</small></a>
</div>
</body>
</html>
`;
write(path.join(base, 'index.html'), chooser);

// remove the old mixed reference tree
for (const f of refPages) {
  fs.rmSync(path.join(refDir, f));
}
fs.rmSync(refDir, { recursive: true, force: true });

console.log('split complete: en/ and zh/ trees written, chooser installed, mixed reference tree removed');
