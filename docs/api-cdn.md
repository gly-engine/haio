# API do CDN

Base: `http://localhost:8080`

## Visão geral

O serviço expõe um único conjunto de rotas HTTP para servir imagens e aplicar transformações na borda.

- Métodos aceitos: `GET` e `HEAD`
- Resposta de sucesso: `200 OK`
- Erros comuns:
  - `400 Bad Request` para rota ou parâmetros inválidos
  - `404 Not Found` para bucket ou recurso ausente
  - `415 Unsupported Media Type` quando o arquivo não é um formato suportado
  - `429 Too Many Requests` quando o limite por IP é atingido
  - `502 Bad Gateway` para falha no upstream
  - `504 Gateway Timeout` para timeout de busca ou conversão

## Rotas

### `GET /cdn/<bucket>/<path>`

Busca um arquivo no bucket nomeado e devolve a imagem original ou uma versão transformada via query string.

Exemplo:

- `/cdn/assets/logo8x8.ppm`
- `/cdn/assets/logo8x8.ppm?format=png`

### `GET /cdn/<path>`

Atalho para o bucket interno `file`, tratado pelo roteador como `/cdn/file/<path>`.

Esse formato só funciona se existir um bucket chamado `file` na configuração.

Exemplo:

- `/cdn/logo8x8.ppm`

### `HEAD`

Qualquer rota `GET` também aceita `HEAD` e retorna os mesmos headers, sem corpo.

## Query string suportada

A query é interpretada como uma pipeline de transformações. A ordem importa.

### `crop=x,y,width,height`

Recorta uma área explícita.

Exemplo:

- `crop=1,2,4,4`

### `resize=WxH` ou `size=WxH`

Redimensiona para uma dimensão exata.

Também aceita porcentagem:

- `resize=30pct`
- `resize=30%`

### `radius=N`

Arredonda os cantos.

### `format=<container>`

Escolhe o formato de saída, por exemplo `png`, `jpg`, `tga`, `ppm`, `gif`, `dds`, `ktx`, `ktx2`.

### `pix_fmt=<color>` ou `pix_format=<color>`

Escolhe a cor interna do container, no estilo do ffmpeg.

Exemplos:

- `pix_fmt=rgb888`
- `pix_format=bgr888`

Se `format` não for informado e apenas `pix_fmt` estiver presente, a resposta volta como pixels crus (`application/octet-stream`).

## Exemplos práticos

- Original: `/cdn/assets/logo8x8.ppm`
- PNG: `/cdn/assets/logo8x8.ppm?format=png`
- Resize: `/cdn/assets/logo8x8.ppm?resize=16x16&format=png`
- Crop + PNG: `/cdn/assets/logo8x8.ppm?crop=1,1,4,4&format=png`
- TGA com cor BGR: `/cdn/assets/logo8x8.ppm?format=tga&pix_fmt=bgr888`

## Imagem pública por URL direta

Se a imagem vier de um link direto e público, o bucket `proxy` permite buscar o host remoto e devolver em outro formato.

Exemplo com a imagem do Canva:

- `/cdn/proxy/marketplace.canva.com/WH2q0/MAG7aTWH2q0/1/tl/canva-um-lindo-e-pequeno-papagaio%2C-o-periquito-sol.-MAG7aTWH2q0.jpg?format=zcis`

Isso só funciona se a URL responder como imagem acessível sem login, cookie ou bloqueio de hotlink. Se o host devolver HTML, erro de acesso, ou redirecionamentos bloqueados, a conversão falha.

## Observações

- A ordem dos parâmetros altera o resultado e também a chave do cache.
- `HEAD` usa o mesmo caminho lógico de `GET`, então um `HEAD` encontra o que um `GET` já armazenou em cache.
- Buckets `file`, `http`, `https`, `s3` e o aberto `https://*` são escolhidos pela URL do bucket no arquivo TOML.
