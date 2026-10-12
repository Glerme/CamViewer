# CamViewer

Visualizador de câmeras IP para o KDE Plasma 6: app com grade de câmeras,
gravação, capturas e alertas de movimento, mais um widget para a área de
trabalho e para o painel.

## Instalar

Os instaladores ficam na [página de releases](https://github.com/Glerme/CamViewer/releases):

| Sistema | Arquivo | Como instalar |
| --- | --- | --- |
| Windows 10/11 | `CamViewer-X.Y.Z-windows-x64-setup.exe` | Executar; não pede administrador |
| Windows (sem instalar) | `CamViewer-X.Y.Z-windows-x64-portable.zip` | Extrair e abrir `CamViewer.exe` |
| Arch, CachyOS, Manjaro | `camviewer-X.Y.Z-1-x86_64.pkg.tar.zst` | `sudo pacman -U arquivo` |
| Debian 13, Ubuntu 25.04+ | `camviewer_X.Y.Z_amd64.deb` | `sudo apt install ./arquivo` |

No Windows o app é o mesmo, com bandeja, gravação, capturas e alertas, e já traz
o FFmpeg. O widget existe só no Linux com KDE Plasma 6.

Depois abra **CamViewer** no menu, adicione uma câmera (endereço `rtsp://…`,
`http://…` ou qualquer coisa que o FFmpeg abra) e, no Plasma, adicione o widget
**CamViewer** à área de trabalho ou ao painel.

### A partir do código

Dependências (Arch/CachyOS): `qt6-multimedia-ffmpeg qt6-declarative qt6-svg
knotifications kstatusnotifieritem qqc2-desktop-style cmake ffmpeg`.

```sh
./install.sh              # instala em ~/.local, sem sudo
./install.sh --autostart  # idem, já com o início no login ligado (também dá para ligar nas Configurações)
./uninstall.sh
```

`cmake -DCAMVIEWER_PORTABLE=ON` compila sem as bibliotecas do KDE e sem o widget
(é o que o Windows usa).

### Publicar uma versão

O GitHub Actions (`.github/workflows/build.yml`) compila tudo a cada push e
guarda os instaladores como artefatos da execução. Uma tag `vX.Y.Z` também cria
a release com eles:

```sh
git tag v0.1.0 && git push origin v0.1.0
```

## Como funciona

```
camviewer (processo único, fica na bandeja)
 ├─ Manager         câmeras, configurações, D-Bus org.camviewer.CamViewer
 ├─ gravação        ffmpeg -c copy → ~/Vídeos/CamViewer/<câmera>/*.mkv
 ├─ capturas        quadro da tela (app) ou ffmpeg (widget) → ~/Imagens/CamViewer
 ├─ MotionDetector  ffmpeg a 2 fps / 320 px, só nas câmeras com alerta ligado
 └─ janela QML      criada ao abrir e destruída ao fechar

plasmoid (roda dentro do plasmashell)
 ├─ vídeo ao vivo com Qt Multimedia, direto da câmera
 └─ lista de câmeras, gravar e capturar: pede ao app pelo D-Bus
```

O widget inicia o app em segundo plano sozinho (ativação D-Bus) quando precisa.

### Consumo de memória

- Uma câmera pausada não tem player: conexão, decodificador e buffers são
  destruídos (`shared/CameraPlayer.qml`).
- **Painel:** só existe vídeo enquanto o miniplayer está aberto.
- **Área de trabalho:** o vídeo para quando uma janela maximizada ou em tela
  cheia cobre a tela (opção do widget).
- **App:** para ao minimizar; opcionalmente ao perder o foco. Fechar a janela
  destrói a interface inteira; sobra o processo de fundo (~40 MB).
- Com um substream cadastrado, a grade, o widget e a detecção de movimento usam
  ele; gravação e captura usam sempre o stream principal.

### Alertas

A detecção compara quadros consecutivos; dois quadros seguidos acima do limite
geram uma notificação com a imagem, e opcionalmente uma gravação automática.
Sensibilidade, intervalo entre alertas e duração da gravação são configuráveis.
Cada câmera com alerta ligado mantém um `ffmpeg` rodando (~80 MB).

## Arquivos

- `~/.config/camviewer/config.json` — câmeras e configurações (modo 600, pois as
  URLs contêm senha).
- `~/.config/plasma-workspace/env/camviewer.sh` — faz o vídeo do widget usar RTSP
  sobre TCP; vale a partir do próximo login.

## D-Bus

```sh
busctl --user call org.camviewer.CamViewer /CamViewer org.camviewer.CamViewer State
busctl --user call org.camviewer.CamViewer /CamViewer org.camviewer.CamViewer Snapshot s <id>
busctl --user call org.camviewer.CamViewer /CamViewer org.camviewer.CamViewer ToggleRecording s <id>
```
