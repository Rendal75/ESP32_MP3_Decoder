
## Audio Pipeline
```
|----------------    -----------    ----------   ------------
| Input_Stream  |----| Decoder |----| Filter |---| Renderer |
|----------------    -----------    ----------   ------------

```


### Input_Stream
Is responsible of retrieve data, from http / Flash / SD card
Interface can be generic so that such Input_Stream could be implemented :
	- RAM buffering
	- SPI FIFO buffering
	- Null Input Stream
An Input Stream declares the format of data it provides (Mime type)

### Decoder
Is responsible of decoding the audio into uncompressed / playable audio
A decoder publishes the list of format (Mime types) it supports

### Filter
Is resonsible to adapt the audio format
Can add audio effects
Can generate directly an audio wave form
Some filters can be chained and seen as a single filter

### Renderer
Is responsible of playing the audio

### Data Flow
The renderer requests audio data to the filter. In practice it request N bytes.
The filter shall provide those N bytes, not less