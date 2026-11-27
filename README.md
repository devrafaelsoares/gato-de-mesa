# Gato de Mesa

Um gatinho que vive na área de trabalho: digita junto com você, usa o mouse, segue o cursor com os olhos e gosta de carinho na cabeça.

## Rodar

```bash
npm install
npm start
```

**Linux (uma vez só):** o gato lê o teclado direto do kernel, porque o Wayland não deixa apps verem teclas globais.

```bash
sudo usermod -aG input $USER   # depois saia e entre na sessão
```

**Windows:** funciona direto (usa `uiohook-napi`).

## Usar

- **Botão direito no gato:** pelagem, criar pelagem, tamanho, cores do teclado e do mouse, modo privado, fundo verde.
- **Arrastar pelo teclado:** move o gato.
- **Passar o mouse várias vezes na cabeça:** carinho.

## Empacotar

```bash
npm run dist   # AppImage no Linux, instalador NSIS no Windows
```

## Arquitetura

```
src/domain          regras puras: comportamento do gato, teclas, pelagens (sem Electron/DOM)
src/application     portas (InputSource, SettingsStore) e InputRelay (entrada crua -> gato)
src/infrastructure  adaptadores: evdev (Linux), uiohook (Windows), config JSON
src/main            composição: janelas, menu, IPC
src/renderer        desenho SVG (CatView), janela do gato e editor de pelagem
```

Nova plataforma = novo `InputSource`. Nova pelagem = um item em `domain/catalog.ts`.

## Versão nativa (C++, leve)

Feita para computadores simples: ~8 MB de memória (a versão Electron usa ~660 MB) e um executável de 500 KB.

```bash
make -C native            # Linux: precisa de libx11-dev e libxext-dev
./native/build/gato-de-mesa
```

Windows: `cmake -S native -B native/out && cmake --build native/out --config Release`, ou baixe o artefato do workflow `native` no GitHub Actions.

```
native/src/core       regras puras: comportamento do gato, teclas, pelagens
native/src/app        portas, InputRelay, menu, CatApp (o que todo sistema compartilha)
native/src/render     rasterizador próprio e CatRenderer (desenho a partir dos sprites)
native/src/platform   linux/ (X11 + evdev) e windows/ (janela em camadas + hooks)
native/assets         sprites gerados pelo `npm run sprites` a partir do desenho em SVG
```

Mudou o desenho em `src/renderer/catView.ts`? Rode `npm run sprites` para gerar os sprites de novo.
