import { CatBrain } from "../domain/cat";
import type { CatView as View } from "../domain/types";
import { desk } from "./bridge";
import { CatView } from "./catView";
import { Purr } from "./purr";

const view = new CatView(document.getElementById("cat")!);
const brain = new CatBrain();
let privacy = false;
const purr = new Purr();

function apply({ settings, skin, devices }: View): void {
  privacy = settings.privacy;
  view.paint(skin, settings.keyboardColor, settings.mouseColor);
  view.showDevices(devices);
  document.body.classList.toggle("green", settings.greenScreen);
}

desk.getView().then(apply);
desk.onView(apply);
desk.onInput((input) => {
  const typed = brain.handle(input);
  if (typed && !privacy) view.bubble(typed);
});
// Drag the cat from anywhere on it
// A click is not a drag: the cat is only "picked up" once the pointer really moves
const PICK_UP_PX = 5;
let drag: { x: number; y: number; moved: boolean } | null = null;
window.addEventListener("pointerdown", (e) => {
  if (e.button !== 0) return;
  drag = { x: e.screenX, y: e.screenY, moved: false };
  document.body.setPointerCapture(e.pointerId);
  desk.dragStart();
});
window.addEventListener("pointermove", (e) => {
  if (!drag) return;
  const dx = e.screenX - drag.x, dy = e.screenY - drag.y;
  if (!drag.moved && Math.hypot(dx, dy) < PICK_UP_PX) return;
  drag.moved = true;
  view.setDragging(true);
  desk.dragMove(dx, dy);
});
window.addEventListener("pointerup", () => {
  if (!drag) return;
  if (drag.moved) view.setDragging(false);
  else view.twitchEars(); // a tap on the cat: it flicks its ears
  drag = null;
  desk.dragEnd();
});
view.onPet(() => {
  if (!drag) brain.stroke();
});

window.addEventListener("contextmenu", (e) => {
  e.preventDefault();
  desk.openMenu();
});

const frame = () => {
  const pose = brain.pose(privacy);
  view.render(pose);
  purr.set(pose.petting);
  requestAnimationFrame(frame);
};
requestAnimationFrame(frame);
