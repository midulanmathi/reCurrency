# reCurrency

a tool to moderate vices with virtues.

## live demo
https://recurrency-demo.onrender.com

## philosophy
"everything in moderation"
reCurrency allows users to sign a **social contract** by doing the following:
1.  **select a vice:** the habit they wish to moderate (e.g., smoking)
2.  **set a schedule:** how often they want to allow it (e.g., once/week)
3.  **define virtues:** productive habits that "buy back" your time (e.g., gym, studying)

the system calculates a personalized **base cost** for every indulgence (sober days). completing your virtue pays it off. 
but, if you exceed 2.5x your base cost, you go **bankrupt** and need a friend to bail you out.

## tech stack
*   **language:** c++17
*   **server:** crow (microframework)
*   **data:** json (persistent volume storage)
*   **frontend:** server-side html/css + chart.js
*   **deployment:** docker + fly.io

## run locally
1.  clone the repo.
2.  run `make`.
3.  run `./recurrency`.
4.  open `localhost:18080`.
