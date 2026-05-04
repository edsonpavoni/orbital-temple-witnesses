# M5Stack Unit-Roller485 Lite — Motor Documentation

SKU **U182-Lite**. Integrated BLDC motor + FOC driver + magnetic encoder + OLED + RS-485/I2C in a 40×40×40mm cube. Used in the First Witness Series artwork.

> Built with [docling](https://github.com/docling-project/docling) — PDFs and HTML rendered to clean Markdown so Claude can read them without base64 bloat.

## Start here

- **[CLAUDE-REFERENCE.md](CLAUDE-REFERENCE.md)** — consolidated cheat sheet (specs, pinout, protocol, commands, Arduino API, safety limits). **Read this first.**

## Folder map

| Path | What it contains |
|------|------------------|
| `CLAUDE-REFERENCE.md` | Consolidated, AI-friendly reference (hand-authored) |
| `processed/` | Markdown produced by docling from each source |
| `sources/` | Original PDFs and HTML (preserved as-is) |
| `repos/M5Unit-Roller/` | Official Arduino library (cloned `--depth=1`) |
| `repos/M5Unit-Roller485-Internal-FW/` | Factory firmware source (cloned `--depth=1`) |
| `.venv/` | Docling Python environment (uv-managed, Python 3.12) |

## Processed docs

| File | Source | Notes |
|------|--------|-------|
| `01_shop_product_page.md` | [Shop page](https://shop.m5stack.com/products/roller485-lite-unit-without-slip-ring-stm32) | Via WebFetch (Shopify blocked curl) |
| `docs_page.md` | [Wiki page](https://docs.m5stack.com/en/unit/Unit-Roller485%20Lite) | **Pinmap, specs, warnings** |
| `TLI5012BE1000.md` | [Datasheet PDF](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/unit/Unit-Roller485/TLI5012BE1000.pdf) | Infineon angle sensor |
| `DRV8311HRRWR.md` | [Datasheet PDF](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/unit/Unit-Roller485/DRV8311HRRWR.pdf) | TI 3-phase FOC motor driver |
| `Unit_Roller485_I2C_Protocol_EN.md` | [Protocol PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/776/Unit-Roller485-I2C-Protocol-EN.pdf) | I2C register map |
| `Unit_Roller485_RS485_Protocol_EN.md` | [Protocol PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/776/Unit-Roller485-RS485-Protocol-EN.pdf) | RS-485 packet protocol |
| `sch_Unit-Roller485_V1.0.md` | [Schematic PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/776/sch_Unit-Roller485_V1.0.pdf) | Hardware schematic (low OCR value) |

## Reprocessing

```bash
cd "/Users/edsonpavoni/Library/CloudStorage/Dropbox/0000 AI/projects/artworks/first-witness-series/code/documentation/motor-roller485-lite"
source .venv/bin/activate
docling sources/<file>.pdf --to md --image-export-mode placeholder --output processed/
```

Use `--image-export-mode placeholder` for text-only Markdown (default embeds base64 images, inflating files ~30×).

## Regenerating from scratch

1. `brew install uv`
2. `uv venv --python 3.12 .venv && source .venv/bin/activate`
3. `uv pip install docling`
4. Re-run the curl + docling commands above for each source.
