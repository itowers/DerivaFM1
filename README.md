# DERIVA

Groovebox generativa de música eletrônica para o M-VAVE FM-1, feita com [foma](https://github.com/konsumer/foma).

*A generative groovebox for the M-VAVE FM-1: seven styles know where their genre puts the notes, two knobs
(density, chaos) redraw six tracks from that knowledge, and every step stays editable by hand.*

**Estado: 0.4, experimental.** O código roda no emulador (compilado para WebAssembly e testado sem interface),
mas ainda **não foi compilado para o chip nem instalado em um FM-1**. Instalar firmware é por sua conta e risco;
se o FM-1 não ligar, segure OCT- ao ligar (resgate por USB) e instale de novo pela página.

## O que tem

- 6 trilhas de até 16 passos: kick, caixa/clap, hat, percussão, baixo, synth. Tudo sintetizado, sem samples.
- 7 estilos (House, Techno, Funk BR, DnB, Trap, Garage, Electro). Cada um guarda a chance de cada passo soar
  em cada trilha, além de andamento, swing, tom, escala e timbres.
- DENS e CAOS redesenham as trilhas destravadas a partir dos mesmos dados; REC rola os dados de novo.
- Por trilha: timbre (kits 808, 909 e lo-fi; baixo FM, reese, 808...; synth sino FM, acorde, pad...), volume,
  comprimento próprio (1-16 passos, polimetria) e modo euclidiano (N batidas espalhadas pelos passos).
- Por passo: chance (100/75/50/25 %), repetições (x1-x3) e travas de filtro (ou decaimento, na bateria) e volume.
- Modo teclado: as 27 teclas tocam a trilha escolhida na escala; REC grava ao vivo (ou passo a passo, parado).
- Performance: sem grave, build-up, FILL (segurar), loops de 1/4 e 1/8; FX + teclas 1-4: stutter, tape stop,
  bitcrush, echo throw.
- Efeitos: filtro DJ, eco, reverb, sidechain do kick, teto suave no master.
- 4 padrões (A-D) que trocam na virada do compasso; modo música com até 16 partes; 3 projetos na memória.
- MIDI: manda o clock; pode seguir um clock de fora (EXT) e mandar as notas (bateria no canal 10, baixo no 1,
  synth no 2).
- Tela: os anéis (uma trilha por anel) enquanto toca; a grade ao editar ou parado.

## Controles

| Controle | Função |
| --- | --- |
| Teclas 1-16 | Passos da trilha selecionada (ligam ao soltar). Segure um passo e gire: KNOB1 chance, KNOB2 repetições, KNOB3 filtro/decaimento, KNOB4 volume, ALGORITHM nota |
| Teclas 17-22 | Selecionar trilha. Com SEL: gerar a trilha de novo. Com FX: silenciar. Com EDIT: travar |
| Teclas 23-27 | Sem grave, build-up, FILL (segurar), loop 1/4, loop 1/8 (segurar) |
| FX + teclas 1-4 | Stutter, tape stop, bitcrush, echo throw, enquanto segurar |
| PRESETS | Estilo |
| SELECT | Andamento (70-180) |
| ALGORITHM | Gira a trilha dentro do seu comprimento (ou a nota do passo segurado) |
| ENV | DENS, CAOS, FILT (meio = desligado), ECO |
| LFO | Trilha escolhida: SOM (timbre), NIVL, PASS (comprimento), EUCL (batidas euclidianas, -- desliga) |
| GLO | TOM, ESCL, SWNG, CORT. De novo: REVB, DUCK, FDBK, MIDI (INT, EXT, INT+N, EXT+N) |
| PLAY | Tocar / parar |
| REC | Gerar tudo. No modo teclado: gravar. Na música: ligar |
| ARP | Toque: variar. Segure: modo teclado (toque de novo para sair) |
| SEQ | Toque: variar sozinho a cada 2 compassos. Segure: tela da música (teclas 1-16 trocam a parte; segure + ALGORITHM: compassos; REC liga; SEQ volta) |
| HOME | Desfazer (de novo: refazer) |
| SAVE | Salvar os 3 projetos (só parado). Segure SAVE + OCT-/OCT+: trocar de projeto |
| OCT- / OCT+ | Padrão anterior / próximo (A-D). Um padrão novo nasce como variação do atual |

## Testar e instalar sem instalar nada

1. Crie um repositório no GitHub com estes arquivos (branch `main`).
2. Em Settings → Pages → Source, escolha **GitHub Actions**.
3. A cada push, o GitHub compila o firmware e publica uma página com o emulador (o FM-1 no navegador, com som)
   e o instalador (Chrome ou Edge, FM-1 no USB). Uma tag (`v0.1`) também cria um release com o `.fwsc`.

Com Docker no computador: `./fm1 dev` e abra http://localhost:8000.

## Antes de publicar no Firmware Hub

- **Identidade**: `firmware.json` usa `FM-1_31` (o pacote sai como `FM-1_31` + o último dígito da versão menor, por exemplo `FM-1_314` na 0.4).
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
