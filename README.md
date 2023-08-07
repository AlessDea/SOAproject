# SOAproject
## Block-level data management service
This specification is related to a Linux device driver implementing block-level maintenance of user data, in particular of user messages. A block of the block-device has size 4KB and its layout is organized as follows:
- the lower half (X bytes) keeps user data;
- the upper half (4KB-X bytes) keeps metadata for the management of the device.

Clearly, the lower the value of X the better since larger messages can be actually managed by software.

The device driver is essentially based on system-calls partially supported by the VFS and partially not. The following VFS non-supported system calls are requested to be designed and implemented:

- `int put_data(char * source, size_t size)` used to put into one free block of the block-device size bytes of the user-space data identified by the source pointer, this operation must be executed all or nothing; the system call returns an integer representing the offset of the device (the block index) where data have been put; if there is currently no room available on the device, the service should simply return the ENOMEM error;
- `int get_data(int offset, char * destination, size_t size)` used to read up to size bytes from the block at a given offset, if it currently keeps data; this system call should return the amount of bytes actually loaded into the destination area or zero if no data is currently kept by the device block; this service should return the ENODATA error if no data is currently valid and associated with the offset parameter.
- `int invalidate_data(int offset)` used to invalidate data in a block at a given offset; invalidation means that data should logically disappear from the device; this service should return the ENODATA error if no data is currently valid and associated with the offset parameter.

When putting data, the operation of reporting data on the device can be either executed by the page-cache write back daemon of the Linux kernel or immediately (in a synchronous manner) depending on a compile-time choice.

The device driver should support file system operations allowing the access to the currently saved data:

- `open` for opening the device as a simple stream of bytes
- `release` for closing the file associated with the device
- `read` to access the device file content, according to the order of the delivery of data. A read operation should only return data related to messages not invalidated before the access in read mode to the corresponding block of the device in an I/O session.

The device should be accessible as a file in a file system supporting the above file operations. At the same time the device should be mounted on whichever directory of the file system to enable the operations by threads. For simplicity, it is assumed that the device driver can support a single mount at a time. When the device is not mounted, not only the above file operations should simply return with error, but also the VFS non-supported system calls introduced above should return with error (in particular with the ENODEV error).

The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time. A block-device layout (its partition) can actually keep up to NBLOCKS blocks or less. If it keeps more than NBLOCKS blocks, the mount operation of the device should fail. The user level software to format the device for its usage should also be designed and implemented.



### Idee
Creazione:
- uso il singlefilefs di quaglia. In pratica esso viene montato su un block device, quindi tutte le operazioni verranno fatte dal layer a blocchi in automatica, non c'è bisogno di fare altro.
- bisogna solo creare quanti blocchi si necessitano e gestire la logica di lettura di essi.
Syscall:
- bisogna creare le syscall ed inserirle nella syscall table con il modulo del Prof
- come fa la syscall a prendere le informazioni del device? devo far si che siano visibili ad esse


Mappa del device: c'è bisogno di una mappa che tenga traccia dei blocchi validi e non. Inoltre c'è bisogno di mantenere l'ordine temporale dei messaggi:.
Per mantenere l'ordine temporale devo eliminare dalla RCU i blocchi che vengono invalidati. In questo modo ogni volta che viene fatta una put_data viene creato un nuovo elemento nella RCU e inserito nell'ordine corretto.


Per il `put_data` e `invalidate_data` bisogna far si che la modifica sul device venga fatta subito dopo che scade il grace period.Questo è garantito
dal fatto che si rimuove dalla lista il blocco subito dopo lo scadere del grace period.


#### N.B. 
> I metadati nel blocco servono solo in caso di crash del modulo. Nel senso che servono come consistenza
> per quelli che si trovano nella RCU list. Infatti i metadati vengono usati solo nella RCU list per verificare
> le varie condizioni necessarie per le operazioni. Questi vengono sempre riportati all'interno del blocco 'fisico' 
> ma non durettamente usati in esso.
> 
> Bisogna infatti implementare una fase di training in cui il modulo va a leggere i blocchi del device e ricostruisce
la RCU list da quelle informazioni.

La struttura è la seguente:

```
                                Block 2     Block 3      Block 4
---------------------------------------------------------------------
| Block 0    | Block 1      | File meta  |  File meta  | File meta  | 
| Superblock | inode of the | ---------- | ----------- | ---------- | 
|            | File         | File data  |  File data  | File data  |
---------------------------------------------------------------------

```

### read syscall
Questa syscall deve ritornare il contenuto del device file in ordine di arrivo dei messaggi. Solo i blocchi non invalidati però devono essere restituiti all'utente.
Il problema è che se la lettura avviene ad un offset che cade su un blocco non valido?

#### Soluzione:
Devo considerare il file come la sequenza di messaggi validi, non devo quindi gestire questa operazione come se sia fata sul device.
Questo implica che devo aggiornare la lunghezza del file ad ogni put data e ad ogni invalidate. Questo si fa andando a scrivere nel blocco 1 del device, nel campo size.
Quindi devo scrivere una funzione che legge l'inode dalla cache, coe viene fatto in `onefilefs_lookup`.


### N.B.
> Bisogna gestire la concorrenza per le operazioni fatte sui blocchi per aggiornare la taglia dei file


# Modifiche da fare

1. =FATTO=: nel make file inserire un'opzione che permetta di non inizializzare `image`, in modo che il device sia persistente. Infatti la prima volta viene fatto:
```Makefile
create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./singlefilemakefs image
	mkdir mount
```
Ma se lo si vuole persistente, ovvero ai prossimi mount si vuole ritrovare il contenuto precedente, allora le prime due righe vanno eliminate in modo da non sovrascrivere il device.
In particolare bisogna fare in modo che create-fs venga chiamato solamente se il silesystem non esiste. In caso non esista bisogna solo far `mount-fs` e a questo punto nel mount bisogna fare dei passaggi di controllo per leggere le informazioni sul block device che erano state memorizzate in precedenza.

Per verificare che il filesystem esista basta verificare che il module `singlefilefs` sia caricato. 


2. create-fs deve essere chiamato solo la prima volta che viene creato il device driver, poichè nelle altre circostanze esiste già il device driver e si vuole probabilmente continuare con quello. In caso chiedere all'utente