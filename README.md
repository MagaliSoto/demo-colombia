# demo-colombia
## Clonar Repo:
```bash
git clone git@github.com:MagaliSoto/demo-colombia.git
```

## Levantar deepstream:

1. `xhost +`

2. `docker compose --profile deepstream up -d --build --force-recreate`


## Levantar backend
```bash
docker compose up -d
```

## Modificaciones 

Modificar rtsp
/perception/cfg/sources.yml

Modificar roi
/perception/cfg/nvdsanalytics.txt
