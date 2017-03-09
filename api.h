// abstract class
class api {
  protected:
  public:
    virtual void init();
    void add(api tmp);
}


class http_api : public api {

}
