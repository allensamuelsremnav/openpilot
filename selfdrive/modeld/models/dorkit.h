// My override
void dorkit(ModelState *s, PubMaster& pm, MessageBuilder& msg, cereal::ModelDataV2::Builder &framed, const ModelOutput& net_outputs);
// My fill function
void fill_model(ModelState *s, cereal::ModelDataV2::Builder &framed, const ModelOutput &net_outputs);
