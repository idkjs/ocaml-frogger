open Js_of_ocaml;
module Html = Dom_html;

let document = Html.window##.document;

let jsopt_value_exn = x => Js.Opt.get(x, () => assert(false));

let jsoptdef_value_exn = x => Js.Optdef.get(x, () => assert(false));
