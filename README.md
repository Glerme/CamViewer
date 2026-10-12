# CamViewer

Visualizador de câmeras IP para o KDE Plasma 6: app com grade de câmeras,
gravação, capturas e alertas de movimento, mais um widget para a área de
trabalho e para o painel.

## Instalar

Dependências (Arch/CachyOS): `qt6-multimedia-ffmpeg qt6-declarative knotifications
kstatusnotifieritem kirigami libplasma extra-cmake-modules cmake ffmpeg`.

```sh
./install.sh              # instala em ~/.local, sem sudo
./install.sh --autostart  # idem, já com o início no login ligado (também dá para ligar nas Configurações)
./uninstall.sh
```

Depois abra **CamViewer** no menu, adicione uma câmera (endereço `rtsp://…`,
`http://…` ou qualquer coisa que o FFmpeg abra) e adicione o widget
**CamViewer** à área de trabalho ou ao painel.

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
