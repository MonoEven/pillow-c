/* pillow-c bilingual docs — language switcher (both / EN / 中文) */
(function () {
  "use strict";
  var root = document.documentElement;
  var saved = null;
  try { saved = localStorage.getItem("pillow-c-lang"); } catch (e) { /* ignore */ }
  if (saved === "en" || saved === "zh" || saved === "both") {
    root.setAttribute("data-lang", saved);
  }
  function refresh() {
    var current = root.getAttribute("data-lang") || "both";
    document.querySelectorAll(".lang-switch button").forEach(function (btn) {
      btn.classList.toggle("active", btn.getAttribute("data-lang") === current);
    });
  }
  document.querySelectorAll(".lang-switch button").forEach(function (btn) {
    btn.addEventListener("click", function () {
      var lang = btn.getAttribute("data-lang");
      root.setAttribute("data-lang", lang);
      try { localStorage.setItem("pillow-c-lang", lang); } catch (e) { /* ignore */ }
      refresh();
    });
  });
  refresh();
})();
