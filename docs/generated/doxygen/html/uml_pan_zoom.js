// Pan/zoom behavior for .uml-viewer elements on the architecture diagrams page.
// Self-contained, no external dependencies (see architecture_diagrams.dox).
(function () {
    "use strict";

    var MIN_SCALE = 0.1;
    var MAX_SCALE = 12;

    function clamp(v, lo, hi) {
        return Math.min(hi, Math.max(lo, v));
    }

    function initViewer(viewer) {
        var inner = viewer.querySelector(".uml-viewer-inner");
        var img = inner ? inner.querySelector("img") : null;
        if (!inner || !img) {
            return;
        }

        var state = { scale: 1, x: 0, y: 0, fitScale: 1 };
        var dragging = false;
        var lastX = 0;
        var lastY = 0;

        function apply() {
            inner.style.transform =
                "translate(" + state.x + "px, " + state.y + "px) scale(" + state.scale + ")";
        }

        function fitToView() {
            var vw = viewer.clientWidth;
            var vh = viewer.clientHeight;
            var iw = img.naturalWidth || img.width || vw;
            var ih = img.naturalHeight || img.height || vh;
            if (!iw || !ih) {
                return;
            }
            var scale = Math.min(vw / iw, vh / ih) * 0.95;
            scale = clamp(scale, MIN_SCALE, MAX_SCALE);
            state.fitScale = scale;
            state.scale = scale;
            state.x = (vw - iw * scale) / 2;
            state.y = (vh - ih * scale) / 2;
            apply();
        }

        function zoomAt(cx, cy, factor) {
            var newScale = clamp(state.scale * factor, MIN_SCALE, MAX_SCALE);
            if (newScale === state.scale) {
                return;
            }
            // Keep the point under the cursor (cx, cy) fixed while scaling.
            var ratio = newScale / state.scale;
            state.x = cx - (cx - state.x) * ratio;
            state.y = cy - (cy - state.y) * ratio;
            state.scale = newScale;
            apply();
        }

        if (img.complete && img.naturalWidth) {
            fitToView();
        } else {
            img.addEventListener("load", fitToView);
        }
        window.addEventListener("resize", fitToView);

        viewer.addEventListener(
            "wheel",
            function (ev) {
                ev.preventDefault();
                var rect = viewer.getBoundingClientRect();
                var cx = ev.clientX - rect.left;
                var cy = ev.clientY - rect.top;
                var factor = ev.deltaY < 0 ? 1.15 : 1 / 1.15;
                zoomAt(cx, cy, factor);
            },
            { passive: false }
        );

        viewer.addEventListener("pointerdown", function (ev) {
            dragging = true;
            lastX = ev.clientX;
            lastY = ev.clientY;
            viewer.classList.add("uml-grabbing");
            viewer.setPointerCapture(ev.pointerId);
        });

        viewer.addEventListener("pointermove", function (ev) {
            if (!dragging) {
                return;
            }
            state.x += ev.clientX - lastX;
            state.y += ev.clientY - lastY;
            lastX = ev.clientX;
            lastY = ev.clientY;
            apply();
        });

        function endDrag(ev) {
            dragging = false;
            viewer.classList.remove("uml-grabbing");
            if (ev && ev.pointerId !== undefined && viewer.hasPointerCapture(ev.pointerId)) {
                viewer.releasePointerCapture(ev.pointerId);
            }
        }
        viewer.addEventListener("pointerup", endDrag);
        viewer.addEventListener("pointercancel", endDrag);
        viewer.addEventListener("pointerleave", function () {
            if (dragging) {
                dragging = false;
                viewer.classList.remove("uml-grabbing");
            }
        });

        viewer.addEventListener("dblclick", function (ev) {
            ev.preventDefault();
            fitToView();
        });

        var zoomInBtn = viewer.querySelector("[data-uml-zoom-in]");
        var zoomOutBtn = viewer.querySelector("[data-uml-zoom-out]");
        var resetBtn = viewer.querySelector("[data-uml-reset]");
        if (zoomInBtn) {
            zoomInBtn.addEventListener("click", function () {
                zoomAt(viewer.clientWidth / 2, viewer.clientHeight / 2, 1.3);
            });
        }
        if (zoomOutBtn) {
            zoomOutBtn.addEventListener("click", function () {
                zoomAt(viewer.clientWidth / 2, viewer.clientHeight / 2, 1 / 1.3);
            });
        }
        if (resetBtn) {
            resetBtn.addEventListener("click", fitToView);
        }
    }

    function init() {
        var viewers = document.querySelectorAll(".uml-viewer[data-uml-viewer]");
        for (var i = 0; i < viewers.length; i++) {
            initViewer(viewers[i]);
        }
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", init);
    } else {
        init();
    }
})();
