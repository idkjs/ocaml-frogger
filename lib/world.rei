open Game;

type t;

let make: unit => t;

let tick: t => t;

let handle_input: (t, Key.t) => t;

let handle_event: (t, Event.t) => t;

let draw: t => Display_list.t;

let finished: t => bool;
