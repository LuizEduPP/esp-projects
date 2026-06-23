# Folio Brain — Arquitetura SOLID

`scripts/lib/` contém **apenas pastas**. Cada pasta expõe API via `index.mjs`.

## Regra de layout

```
lib/<domínio>/index.mjs     ← API pública do domínio
lib/<domínio>/*.mjs         ← implementação (ou subpastas com index.mjs)
```

Sem `.mjs` solto na raiz de `lib/`.

## Árvore

```
lib/
├── config/index.mjs
├── locale/index.mjs
├── models/index.mjs         # ModelSlot, modelId, endpoints
├── db/
│   ├── index.mjs
│   ├── schema.mjs, connection.mjs
│   └── *.mjs                # repositórios por entidade
├── util/
│   ├── index.mjs
│   ├── time.mjs, audio.mjs, response.mjs, json.mjs
├── stt/
│   ├── index.mjs
│   ├── whisper-service.mjs, audio-gate.mjs
├── llm/
│   ├── index.mjs
│   ├── client.mjs, vision.mjs
├── memory/
│   ├── index.mjs
│   ├── lexical.mjs, embeddings.mjs, retrieval.mjs, indexing.mjs
├── services/
│   ├── index.mjs            # re-exporta ingest, pipeline, digest
│   ├── ingest/index.mjs
│   ├── pipeline/index.mjs
│   └── digest/index.mjs
└── http/
    ├── index.mjs
    └── server.mjs
```

## Imports (entry points)

```javascript
import { CFG } from "./lib/config/index.mjs";
import { openDb } from "./lib/db/index.mjs";
import { runDigestForDay, startProcessingLoop } from "./lib/services/index.mjs";
import { createFolioServer } from "./lib/http/index.mjs";
import { retrieveMemories } from "./lib/memory/index.mjs";
import { modelId, ModelSlot } from "./lib/models/index.mjs";
```

## Fluxo

```
ESP32 → services/ingest → db/ → services/pipeline → services/digest → memory/
```
