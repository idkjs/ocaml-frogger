open! Base;
open! Js_of_ocaml;
open! Scaffold.Import;
open Scaffold.Draw;
open Frogger.Game;

let draw_background = (screen, _config: Frogger.Config.t, wad: Wad.t) =>
  Image_impl.draw(
    screen,
    wad.background,
    0.,
    0.,
    screen.width,
    screen.height,
  );

let render =
    (
      config,
      wad,
      screen: Screen.t,
      dl_current: Display_list.t,
      dl_next: Display_list.t,
      alpha: float,
    ) => {
  draw_background(screen, config, wad);
  let m = config.grid_size_in_px;
  let grid_to_screen = (p: Position.t) => (
    p.x * m,
    (config.num_rows - p.y - 1) * m,
  );

  let interpolate_x = (cur, next) =>
    /* This isn't quite perfect: really, we should draw two copies, not just
       one, when the sprite is wrapping around the edge. Left as an exercise for
       the student! */
    if (Int.abs(cur - next) < (config.num_cols - 1) * m) {
      (1. -. alpha) *. Int.to_float(cur) +. alpha *. Int.to_float(next);
    } else {
      let next =
        if (next > cur) {
          next - config.num_cols * m;
        } else {
          next + config.num_cols * m;
        };

      (1. -. alpha) *. Int.to_float(cur) +. alpha *. Int.to_float(next);
    };

  let interpolate_y = (cur, next) =>
    (1. -. alpha) *. Int.to_float(cur) +. alpha *. Int.to_float(next);

  List.iter(
    List.zip_exn(dl_current, dl_next),
    ~f=(((image_cur, pos_cur), (_image_next, pos_next))) => {
      let (x_cur, y_cur) = grid_to_screen(pos_cur);
      let (x_next, y_next) = grid_to_screen(pos_next);
      let x = interpolate_x(x_cur, x_next);
      let y = interpolate_y(y_cur, y_next);
      Image_impl.draw(screen, Wad.lookup_image(wad, image_cur), x, y, m, m);
    },
  );
};

module Game_impl = {
  type t('world) = {
    init: 'world,
    handle_event: ('world, Event.t) => 'world,
    draw: 'world => Display_list.t,
    finished: 'world => bool,
  };
};

module Scaffold_state = {
  type t('world) = {
    world_cur: 'world,
    world_next: option('world),
    interpolation_alpha: float,
    should_interpolate: bool,
    history: list('world),
  };

  let create = (world_init: 'world) => {
    world_cur: world_init,
    world_next: None,
    interpolation_alpha: 0.,
    should_interpolate: false,
    history: [],
  };

  let render = (config, wad, screen, game_impl: Game_impl.t('world), t) => {
    let world_next = Option.value(~default=t.world_cur, t.world_next);
    let alpha =
      if (t.should_interpolate) {
        t.interpolation_alpha -. 0.5;
      } else {
        0.;
      };

    render(
      config,
      wad,
      screen,
      game_impl.draw(t.world_cur),
      game_impl.draw(world_next),
      alpha,
    );
  };

  let set_world_next_if_interpolating = (game_impl: Game_impl.t('world), t) => {
    let world_next =
      if (t.should_interpolate) {
        Some(game_impl.handle_event(t.world_cur, Tick));
      } else {
        /* Do not call [game_impl.handle_event] when not interpolating,
           otherwise student implementations using mutable state will behave
           unexpectedly. */
        t.world_next;
      };

    {...t, world_next};
  };

  let set_world_cur_and_save_history =
      (game_impl: Game_impl.t('world), world_cur, t) => {
    let history =
      if (game_impl.finished(world_cur)) {
        t.history;
      } else {
        [t.world_cur, ...t.history];
      };

    {...t, world_cur, history};
  };

  let apply_tick = (game_impl: Game_impl.t('world), t) => {
    let world_cur = game_impl.handle_event(t.world_cur, Tick);
    set_world_next_if_interpolating(
      game_impl,
      set_world_cur_and_save_history(
        game_impl,
        world_cur,
        {...t, interpolation_alpha: 0.},
      ),
    );
  };

  let toggle_interpolation = (game_impl: Game_impl.t('world), t) =>
    set_world_next_if_interpolating(
      game_impl,
      {...t, should_interpolate: !t.should_interpolate},
    );

  let apply_keypress = (game_impl: Game_impl.t('world), t, which_key) => {
    let world_cur = game_impl.handle_event(t.world_cur, Keypress(which_key));
    set_world_next_if_interpolating(
      game_impl,
      set_world_cur_and_save_history(game_impl, world_cur, t),
    );
  };

  let undo = (game_impl: Game_impl.t('world), t) =>
    switch (t.history) {
    | [] => {...t, interpolation_alpha: 0.}
    | [x, ...xs] =>
      set_world_next_if_interpolating(
        game_impl,
        {...t, world_cur: x, history: xs, interpolation_alpha: 0.},
      )
    };
};

let init_event_handlers =
    (
      config: Frogger.Config.t,
      screen: Screen.t,
      wad,
      game_impl: Game_impl.t(_),
    ) => {
  let scaffold_state = ref(Scaffold_state.create(game_impl.init));
  let _ =
    Html.window##setInterval(
      Js.wrap_callback(() =>{
        let () =
          Scaffold_state.render(
            config,
            wad,
            screen,
            game_impl,
            scaffold_state^,
          );

        let new_interpolation_alpha =
          scaffold_state^.interpolation_alpha
          +. config.render_interval_ms
          /. config.logic_interval_ms;

        scaffold_state :=
          (
            if (Float.(>=)(new_interpolation_alpha, 1.0)) {
              Scaffold_state.apply_tick(game_impl, scaffold_state^);
            } else {
              {
                ...scaffold_state^,
                interpolation_alpha: new_interpolation_alpha,
              };
            }
          );
      }),
      config.render_interval_ms,
    );

  Html.window##.onkeydown :=
    Dom.handler(key_event =>{
      let new_scaffold_state = cur_scaffold_state => {
        let handle_keypress = which_key =>
          Scaffold_state.apply_keypress(
            game_impl,
            cur_scaffold_state,
            which_key,
          );

        let key = jsoptdef_value_exn(key_event##.key);
        switch (Js.to_string(key)) {
        | "ArrowUp" => handle_keypress(Arrow_up)
        | "ArrowDown" => handle_keypress(Arrow_down)
        | "ArrowLeft" => handle_keypress(Arrow_left)
        | "ArrowRight" => handle_keypress(Arrow_right)
        | "i" =>
          Scaffold_state.toggle_interpolation(game_impl, cur_scaffold_state)
        | "u" => Scaffold_state.undo(game_impl, cur_scaffold_state)
        | _ => cur_scaffold_state
        };
      };

      scaffold_state := new_scaffold_state(scaffold_state^);
      Js._true;
    });
};

let create_canvas = (config: Frogger.Config.t) => {
  let canvas = Html.createCanvas(document);
  canvas##.width := Frogger.Config.width(config);
  canvas##.height := Frogger.Config.height(config);
  canvas;
};

let initialize_and_main_loop =
    (config: Frogger.Config.t, wad, game: Game_impl.t(_)) => {
  let board_div =
    jsopt_value_exn(document##getElementById(Js.string("board")));

  let canvas = create_canvas(config);
  Dom.appendChild(board_div, canvas);
  let context = canvas##getContext(Html._2d_);
  let screen = {
    Screen.context,
    width: Frogger.Config.width(config),
    height: Frogger.Config.height(config),
  };

  init_event_handlers(config, screen, wad, game);
};

let main = game => {
  /* It's important to load assets here rather than in the onload handler, so
     that the handler runs after they're loaded. This is an okay thing to do for
     a simple program like this which knows all the things it needs to load at
     startup.
     When that's not a tenable strategy, you must wait for images (and/or other
     assets) to load before using them. While this _can_ be done with callbacks,
     a monadic concurrency library (conceptually related to futures and
     promises) is the idiomatic way to handle that in OCaml. */
  let wad = Wad.create(Frogger.Config.default);
  Html.window##.onload :=
    Html.handler(_ =>{
      let () = initialize_and_main_loop(Frogger.Config.default, wad, game);
      Js._false;
   } );
};

let () = {
  let game =
    Frogger.World.{Game_impl.init: make(), handle_event, draw, finished};

  main(game);
};
