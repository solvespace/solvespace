module.exports = {
  rules: {
    "color-no-invalid-hex": true,
    "font-family-no-duplicate-names": true,
    "font-family-no-missing-generic-family-keyword": true,
    "function-calc-no-unspaced-operator": true,
    "function-linear-gradient-no-nonstandard-direction": true,
    "string-no-newline": true,
    "unit-no-unknown": true,
    "property-no-unknown": [
      true,
      {
        ignoreProperties: [
          "/^gtk-/",
          "/^-gtk-/",
          "min-width",
          "min-height",
          "caret-color",
          "selection-background-color",
          "selection-color"
        ]
      }
    ],
    "keyframe-declaration-no-important": true,
    "declaration-block-no-duplicate-properties": true,
    "declaration-block-no-shorthand-property-overrides": true,
    "block-no-empty": true,
    "selector-pseudo-class-no-unknown": [
      true,
      {
        ignorePseudoClasses: ["active", "checked", "disabled", "focus", "hover", "selected"]
      }
    ],
    "selector-pseudo-element-no-unknown": [
      true,
      {
        ignorePseudoElements: ["selection"]
      }
    ],
    "selector-type-no-unknown": [
      true,
      {
        ignoreTypes: [
          "button",
          "entry",
          "grid",
          "headerbar",
          "label",
          "menubar",
          "menuitem",
          "popover",
          "scrollbar",
          "slider",
          "window"
        ]
      }
    ],
    "media-feature-name-no-unknown": true,
    "comment-no-empty": true,
    "no-duplicate-at-import-rules": true,
    "no-duplicate-selectors": true,
    "no-empty-source": true,
    "no-extra-semicolons": true,
    "no-invalid-double-slash-comments": true,
    
    "at-rule-no-unknown": [
      true,
      {
        ignoreAtRules: ["define-color", "import", "keyframes"]
      }
    ],
    "selector-class-pattern": null, // Allow GTK naming conventions
    "selector-id-pattern": null, // Allow GTK naming conventions
    
    "color-hex-case": "lower",
    "function-comma-space-after": "always",
    "function-comma-space-before": "never",
    "function-name-case": "lower",
    "function-parentheses-space-inside": "never",
    "number-leading-zero": "always",
    "number-no-trailing-zeros": true,
    "string-quotes": "double",
    "unit-case": "lower",
    "value-keyword-case": "lower",
    "property-case": "lower",
    "declaration-bang-space-after": "never",
    "declaration-bang-space-before": "always",
    "declaration-colon-space-after": "always",
    "declaration-colon-space-before": "never",
    "declaration-block-semicolon-newline-after": "always",
    "declaration-block-semicolon-space-before": "never",
    "declaration-block-trailing-semicolon": "always",
    "block-closing-brace-empty-line-before": "never",
    "block-closing-brace-newline-after": "always",
    "block-closing-brace-newline-before": "always",
    "block-opening-brace-newline-after": "always",
    "block-opening-brace-space-before": "always",
    "selector-attribute-brackets-space-inside": "never",
    "selector-attribute-operator-space-after": "never",
    "selector-attribute-operator-space-before": "never",
    "selector-combinator-space-after": "always",
    "selector-combinator-space-before": "always",
    "selector-descendant-combinator-no-non-space": true,
    "selector-pseudo-class-case": "lower",
    "selector-pseudo-class-parentheses-space-inside": "never",
    "selector-pseudo-element-case": "lower",
    "selector-pseudo-element-colon-notation": "double",
    "selector-type-case": "lower",
    "media-feature-colon-space-after": "always",
    "media-feature-colon-space-before": "never",
    "media-feature-name-case": "lower",
    "media-feature-parentheses-space-inside": "never",
    "media-feature-range-operator-space-after": "always",
    "media-feature-range-operator-space-before": "always",
    "at-rule-name-case": "lower",
    "at-rule-name-space-after": "always",
    "at-rule-semicolon-newline-after": "always",
    "at-rule-semicolon-space-before": "never",
    "comment-whitespace-inside": "always"
  }
};
