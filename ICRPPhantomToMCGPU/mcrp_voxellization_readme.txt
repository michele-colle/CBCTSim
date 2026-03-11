Titolo: mcrp_to_vox — Descrizione e funzionamento

Scopo
- Il programma `mcrp_to_vox.cpp` converte modelli anatomici (phantom) in una rappresentazione voxelata ottimizzata per pipeline di simulazione Monte Carlo su GPU, mantenendo le informazioni di materiale e organo.

Funzionalità principali
- Caricamento dei dati di input (mesh e mappe materiali).
- Voxelizzazione su una griglia 3D secondo la risoluzione scelta dall'utente.
- Conservazione delle etichette anatomiche e materiali per uso quantitativo.
- Esportazione in formati compatibili con strumenti di simulazione ad alte prestazioni.

Principio operativo (breve)
- Lettura: importazione del modello e delle etichette materiali.
- Voxelizzazione: lo spazio viene campionato su una griglia 3D e ad ogni voxel viene assegnato il materiale più rappresentativo.
- Output: file normalizzati e verificati, pronti per l'uso nelle pipeline GPU.

Phantom mesh MCRP vs phantom voxelizzati
- Phantom mesh (MCRP): rappresentazioni basate su elementi (nodi/elementi, superfici 3D) che descrivono con precisione le geometrie anatomiche, tipicamente utilizzate per analisi dettagliate e modellazione strutturale.
- Phantom voxelizzati: rappresentazioni raster 3D con voxel discreti etichettati con materiali; sono la forma più pratica per la maggior parte dei simulatori Monte Carlo basati su GPU.
- Vantaggio della voxelizzazione personalizzata: le versioni voxelate fornite da ICRP spesso impiegano voxel di dimensione relativamente grande. Con `mcrp_to_vox.cpp` è possibile scegliere liberamente la dimensione del voxel, ottenendo risoluzione arbitrariamente fine a fronte di maggiori requisiti di calcolo e spazio su disco.

Riferimenti suggeriti (da verificare)
- ICRP Publication 110 — Adult Reference Computational Phantoms (ICRP, 2009).
- ICRP Publication 89 — Basic Anatomical and Physiological Data for Use in Radiological Protection (ICRP, 2002).
- Letteratura su phantom mesh vs voxel phantom e su metodi di conversione mesh-to-voxel (consultare database scientifici per citazioni complete).

Output e integrazione
- Il risultato della voxelizzazione è direttamente importabile in `mcgpu` e nelle pipeline correlate.
- Il formato di output facilita l'aggiunta di componenti esterni al modello (ad esempio barelle o supporti) e permette di pianificare estensioni future, come l'inserimento di elementi implantari specifici (es. impianti dentali), che sono previsti come sviluppo successivo.

Note pratiche
- Controllare che le etichette materiali nei file di input corrispondano a quelle attese dal processo.
- La scelta della risoluzione di voxel influisce linearmente sul numero di voxel, sui tempi di elaborazione e sulla dimensione dei file di output.

Se desiderate, posso recuperare e inserire citazioni complete e link ai lavori citati; qui ho riportato i riferimenti principali come suggerimento da confermare.

coordinate baricnetriche mesh:
https://www.cdsimpson.net/2014/10/barycentric-coordinates.html