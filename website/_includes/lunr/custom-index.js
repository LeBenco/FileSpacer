{% unless site.lang contains 'en-' %}
// Keep Unicode words, including Greek; English stemming is unsuitable here.
if (i === Object.keys(docs)[0]) {
  const unicodeTrimmer = token => token.update(value => value.replace(/^[^\p{L}\p{M}\p{N}_]+|[^\p{L}\p{M}\p{N}_]+$/gu, ''));
  lunr.Pipeline.registerFunction(unicodeTrimmer, 'fileSpacerUnicodeTrimmer');
  this.pipeline.reset();
  this.pipeline.add(unicodeTrimmer);
  this.searchPipeline.reset();
  this.searchPipeline.add(unicodeTrimmer);
}
{% endunless %}
