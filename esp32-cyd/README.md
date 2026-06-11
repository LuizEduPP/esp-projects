# ESP32-2432S028R (CYD)

Placa **Cheap Yellow Display** — ESP32 com TFT **2.8″ ILI9341**, touch **XPT2046**, slot microSD e LED RGB.

## Projetos

| Projeto | Descrição | Docs |
|---------|-----------|------|
| [**cyd-gb/**](cyd-gb/) | Emulador Game Boy / GBC com launcher de ROMs e controles touch | [README](cyd-gb/README.md) |

## Requisitos

- [PlatformIO](https://platformio.org/) (CLI ou extensão IDE)
- [Yarn](https://yarnpkg.com/) 1.x
- Cartão microSD **FAT32** (para o cyd-gb)

## Quick start

Na raiz do monorepo:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash
yarn cyd-gb:monitor
```

Ou dentro do projeto:

```bash
cd cyd-gb
yarn fw:flash
yarn fw:monitor
```

## Créditos

O firmware **cyd-gb** é derivado de [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) por [artanergin44-collab](https://github.com/artanergin44-collab/cyd-gb). Detalhes em [cyd-gb/README.md](cyd-gb/README.md).
