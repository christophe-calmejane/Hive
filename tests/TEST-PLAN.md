# Connection Matrix

## Glossary
* Stream: Either a STREAM_INPUT/OUTPUT or a REDUNDANT_STREAM_INPUT/OUTPUT descriptor
	* _Simple Stream_: A non-redundant Stream (representation of either STREAM_INPUT or STREAM_OUTPUT)
	* _Redundant Stream Pair_: A pair of 2 Streams that form a virtual Stream (representation of either REDUNDANT_STREAM_INPUT or REDUNDANT_STREAM_OUTPUT)
	* _Primary Stream_: The first Stream of a _Redundant Stream Pair_
	* _Secondary Stream_: The second Stream of a _Redundant Stream Pair_
	* _Redundant Stream_: Either _Primary Stream_ or _Secondary Stream_
* Node: Entry in the Connection Matrix header (either vertical or horizontal)
	* _Entity Node_: An entry that represents an Entity
	* _Stream Node_: An entry that represents a _Simple Stream_
	* _Redundant Node_: An entry that represents a _Redundant Stream Pair_
	* _Primary Redundant Stream Node_: An entry that represents a _Primary Stream_
	* _Secondary Redundant Stream Node_: An entry that represents a _Secondary Stream_
	* _Redundant Stream Node_: An entry that represents either a _Primary Redundant Stream Node_ or a _Secondary Redundant Stream Node_

## Non-redundant Talker and Non-redundant Listener
TODO

## Redundant Talker and Non-Redundant Listener

### Test 1: Connection of a not connected _Simple Stream_

#### Given
* Talker _Redundant Stream_ not connected to Listener _Simple Stream_
* Listener _Simple Stream_ not connected to Talker _Redundant Stream_

#### When
* Clicking on the intersection of a _Redundant Stream Node_ of Talker and the _Stream Node_ of the Listener

#### Then
* ConnectStream() is sent on the matching _Redundant Stream_ of the Talker


### Test 2: Connection of a not connected _Simple Stream_
#### Given
* Talker _Redundant Stream Pair_ not connected at all to Listener _Simple Stream_
* Listener _Simple Stream_ not connected at all to Talker _Redundant Stream Pair_

#### When
* Clicking on the intersection of the _Redundant Node_ of Talker and the _Stream Node_ of the Listener

#### Then
* ConnectStream() is sent on the _Redundant Stream_ of the Talker that is on the same gPTP domain than the Listener (if applicable).
* ConnectStream() is sent on the first _Redundant Stream_ of the Talker which Network Interface is Up (if applicable).
* ConnectStream() is sent on the _Primary Stream_ of the Talker


### Test 3: Disconnection of a connected _Simple Stream_

#### Given
* Talker _Redundant Stream_ connected to Listener _Simple Stream_
* Listener _Simple Stream_ connected to Talker _Redundant Stream_

#### When
* Clicking on the intersection of the _Redundant Stream Node_ of Talker connected to the _Stream Node_ of the Listener

### Or when
* Clicking on the intersection of the _Redundant Node_ of Talker and the _Stream Node_ of the Listener

#### Then
* DisonnectStream() is sent on the matching connected _Redundant Stream_ of the Talker


## Redundant Talker and Redundant Listener

### Test 1: Connection of a not connected _Redundant Stream Pair_

#### Given
* Talker _Redundant Stream Pair_ not connected at all to Listener _Redundant Stream Pair_
* Listener _Redundant Stream Pair_ not connected at all to Talker _Redundant Stream Pair_

#### When
* Clicking on the intersection of the _Redundant Node_ of Talker and Listener

#### Then
* ConnectStream() is sent on both streams of the _Redundant Stream Pair_


### Test 2: Connection of a partially connected _Redundant Stream Pair_

#### Given
* Talker _Redundant Stream Pair_ partially connected to Listener _Redundant Stream Pair_
* Listener _Redundant Stream Pair_ partially connected to Talker _Redundant Stream Pair_

#### When
* Clicking on the intersection of the _Redundant Node_ of Talker and Listener

#### Then
* ConnectStream() is sent on the not connected stream of the _Redundant Stream Pair_


### Test 3: Disconnection of a connected _Redundant Stream Pair_

#### Given
* Talker _Redundant Stream Pair_ is fully connected to Listener _Redundant Stream Pair_
* Listener _Redundant Stream Pair_ is fully connected to Talker _Redundant Stream Pair_

#### When
* Clicking on the intersection of the _Redundant Node_ of Talker and Listener

#### Then
* DisconnectStream() is sent on both streams of the _Redundant Stream Pair_


### Test 4: Partial connection of a _Redundant Stream Pair_ (Primary Stream)

#### Given
* Talker _Primay Stream_ not connected to Listener _Primary Stream_
* Listener _Primay Stream_ not connected to Talker _Primary Stream_

#### When
* Clicking on the intersection of _Primary Redundant Stream Node_ of Talker and Listener

#### Or when
* Clicking on the intersection Talker _Redundant Node_ and Listener _Primary Redundant Stream Node_

#### Or when
* Clicking on the intersection Talker _Primary Redundant Stream Node_ and Listener _Redundant Node_

#### Then
* ConnectStream() is sent on _Primary Stream_


### Test 5: Partial connection of a _Redundant Stream Pair_ (Secondary Stream)
Same test than **Test 4** but with _Secondary Stream_ in place of _Primary Stream_


### Test 6: Partial disconnection of a _Redundant Stream Pair_ (Primary Stream)

#### Given
* Talker _Primay Stream_ connected to Listener _Primary Stream_
* Listener _Primay Stream_ connected to Talker _Primary Stream_

#### When
* Clicking on the intersection of _Primary Redundant Stream Node_ of Talker and Listener

#### Or when
* Clicking on the intersection Talker _Redundant Node_ and Listener _Primary Redundant Stream Node_

#### Or when
* Clicking on the intersection Talker _Primary Redundant Stream Node_ and Listener _Redundant Node_

#### Then
* DisconnectStream() is sent on _Primary Stream_


### Test 7: Partial disconnection of a _Redundant Stream Pair_ (Secondary Stream)
Same test than **Test 6** but with _Secondary Stream_ in place of _Primary Stream_


