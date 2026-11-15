# DERIVA

Groovebox generativa de música eletrônica para o M-VAVE FM-1, feita com [foma](https://github.com/konsumer/foma).

*A generative groovebox for the M-VAVE FM-1: seven styles know where their genre puts the notes, two knobs
(density, chaos) redraw six tracks from that knowledge, and every step stays editable by hand.*

**Estado: 0.5, experimental.** O código roda no emulador (compilado para WebAssembly e testado sem interface),
mas ainda **não foi compilado para o chip nem instalado em um FM-1**. Instalar firmware é por sua conta e risco;
se o FM-1 não ligar, segure OCT- ao ligar (resgate por USB) e instale de novo pela página.

## O que tem

- 6 trilhas de até 64 passos (4 páginas de 16): kick, caixa/clap, hat, percussão, baixo, synth. Tudo sintetizado.
- 19 estilos: Lo-fi, Reggaeton, Synthwave, Amapiano, Disco, Deep, House, Minimal, Electro, Funk BR, Acid, Breaks,
  Techno, Garage, Trance, Dubstep, Trap, Psy, Drum & Bass. Cada um guarda a chance de cada passo soar em cada
  trilha, além de andamento, swing, tom, escala e timbres. PRESETS anda por eles do mais lento ao mais rápido;
  tocando, o novo estilo entra no próximo compasso e o andamento desliza até o novo (o mesmo vale para os padrões
  A-D e as partes da música).
- DENS e CAOS redesenham as trilhas destravadas; REC rola dados novos. Num padrão longo, o último compasso vem
  mais cheio.
- Por trilha: timbre, volume, comprimento (1-64, polimetria) e modo euclidiano.
- Por passo: chance, repetições e travas de filtro (ou decaimento) e volume (15 níveis).
- Tela: os anéis com os quatro knobs nos cantos enquanto toca (dá para girar sem sair dela); a grade para editar;
  HOME alterna.
- FX: segurando, as teclas brancas viram 16 efeitos, com um guia na tela: stutter 1/16 e 1/32, tape stop,
  bitcrush, echo throw, reverb throw, gate, filtro passa-baixa e passa-alta, break, sem grave, build-up, fill,
  loops de 1/4, 1/8 e 1/16.
- Modo teclado (ARP segurado), modo música (SEQ segurado, até 16 partes de até 64 compassos), MIDI (clock e
  notas), 2 projetos.

## Controles

| Controle | Função |
| --- | --- |
| Teclas brancas (16) | Os passos da página mostrada, da trilha escolhida (ligam ao soltar). Segure um passo e gire: KNOB1 chance, KNOB2 repetições, KNOB3 filtro/decaimento, KNOB4 volume, ALGORITHM nota |
| OP1-OP6 (pretas) | As trilhas: kick, caixa, hat, percussão, baixo, synth. Com FX: silenciar. Com SEL: gerar de novo. Com EDIT: travar |
| Pretas 7-10 | Páginas 1-4 (passos 1-16, 17-32, 33-48, 49-64) |
| Preta 11 | x2: cada trilha fica duas vezes mais longa, com a cópia depois (até 64) |
| FX (segurado) | Guia de efeitos na tela; as teclas brancas aplicam cada um enquanto apertadas |
| PRESETS | Estilo (do mais lento ao mais rápido) |
| SELECT | Andamento (desliza tocando) |
| ALGORITHM | Gira a trilha (ou a nota do passo segurado, ou os compassos da parte segurada) |
| ENV | DENS, CAOS, FILT, ECO |
| LFO | Trilha escolhida: SOM, NIVL, PASS, EUCL |
| GLO | TOM, ESCL, SWNG, CORT. De novo: REVB, DUCK, FDBK, MIDI |
| PLAY | Tocar / parar |
| REC | Gerar tudo. No teclado: gravar. Na música: ligar |
| ARP | Toque: variar. Segure: modo teclado (as brancas tocam a escala) |
| SEQ | Toque: variar sozinho. Segure: a música |
| HOME | Toque: anéis ou grade. Segure: desfazer |
| SAVE | Salvar (parado). Segure SAVE + OCT-/OCT+: o outro projeto |
| OCT- / OCT+ | Padrão anterior / próximo (A-D) |

## Testar e instalar sem instalar nada

1. Crie um repositório no GitHub com estes arquivos (branch `main`).
2. Em Settings → Pages → Source, escolha **GitHub Actions**.
3. A cada push, o GitHub compila o firmware e publica uma página com o emulador (o FM-1 no navegador, com som)
   e o instalador (Chrome ou Edge, FM-1 no USB). Uma tag (`v0.1`) também cria um release com o `.fwsc`.

Com Docker no computador: `./fm1 dev` e abra http://localhost:8000.

## Antes de publicar no Firmware Hub

- **Identidade**: `firmware.json` usa `FM-1_31` (o pacote sai como `FM-1_31` + o último dígito da versão menor, por exemplo `FM-1_315` na 0.5).
- Ponha o link do repositório em `web/index.html` (o link Source) — a licença é GPL-3.0.
- `fmhub.json` já está na raiz (texto do cartão). Acrescente `author` e, se quiser, `image`.
- Para o selo "Built from source", o release precisa sair do workflow compartilhado
  [zvenson/fm1-build](https://github.com/zvenson/fm1-build) (veja <https://thefirmwarehub.com/kb/>).
- Teste num FM-1 de verdade o que o hub pede: instalar vindo do V15 e de outro firmware, sair para outro
  firmware, e continuar uma instalação interrompida.
- Este firmware usa a área de ajustes do foma; projetos e samples de outros firmwares não são lidos.

## Código

Tudo em [src/main.c](src/main.c): as tabelas dos estilos, a geração, o som (só inteiros, o chip não tem FPU),
o sequenciador dentro de `audio()` e o painel em `loop()`. A API está em [platform/fm1.h](platform/fm1.h).

## Licença

GPL-3.0-only. Créditos e partes de terceiros: [LICENSING.md](LICENSING.md).
