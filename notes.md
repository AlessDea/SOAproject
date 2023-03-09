
    
| `4096 - X` | `X`        |
|------------|------------|
 | Metadata   | User Data  |
| upper half | lower half |


## make request
La funzione make request è quella che si occupa di piazzare le richieste di I/O nella coda
ed inoltre invoca poi la funzione request. Questa può essere sostituita con una ad hoc, scritta da noi.
In questo modo si può far si di avere scritture dirette senza passare per la request queue.